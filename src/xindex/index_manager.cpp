// src/xindex/index_manager.cpp
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "xbase.hpp"
#include "cdx/cdx_meta.hpp"
#include "cli/path_resolver.hpp"
#include "xindex/index_manager.hpp"

namespace fs = std::filesystem;

namespace xindex {

static inline bool ends_with_icase_(std::string s, std::string suf) {
    if (s.size() < suf.size()) return false;
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char ch){ return static_cast<char>(std::tolower(ch)); });
    std::transform(suf.begin(), suf.end(), suf.begin(),
        [](unsigned char ch){ return static_cast<char>(std::tolower(ch)); });
    return s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

static inline std::string read_all_text_(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static inline std::string json_string_value_(const std::string& doc, const std::string& key) {
    // Minimal: find "key" : "value"
    const std::string needle = "\"" + key + "\"";
    auto pos = doc.find(needle);
    if (pos == std::string::npos) return {};
    pos = doc.find(':', pos + needle.size());
    if (pos == std::string::npos) return {};
    pos = doc.find('"', pos);
    if (pos == std::string::npos) return {};
    auto end = doc.find('"', pos + 1);
    if (end == std::string::npos) return {};
    return doc.substr(pos + 1, end - (pos + 1));
}

std::string IndexManager::to_upper_copy_ascii_(std::string s) {
    for (auto& ch : s) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return s;
}

IndexManager::IndexManager(xbase::DbArea& area) : area_(area) {}

IndexManager::~IndexManager() {
    close();
}

void IndexManager::close() noexcept {
    if (backend_) {
        backend_->close();
        backend_.reset();
    }
    container_path_.clear();
    tag_upper_.clear();
}

bool IndexManager::isCdx() const noexcept {
    return dynamic_cast<const CdxBackend*>(backend_.get()) != nullptr;
}

std::string IndexManager::activeTag() const {
    if (!backend_) return {};
    if (auto* tb = dynamic_cast<ITagBackend*>(backend_.get())) return tb->activeTag();
    return tag_upper_;
}

bool IndexManager::openCdx(const std::string& cdx_container_path,
                           const std::string& tag_upper,
                           std::string* err) {
    close();

    if (cdx_container_path.empty()) {
        if (err) *err = "openCdx: empty path";
        return false;
    }

    // ---- DBF <-> CDX marriage gate -----------------------------------------
    const auto identity = cdxmeta::build_identity(area_);

    auto same_core_shape = [&](const cdxmeta::TableIdentity& a,
                               const cdxmeta::TableIdentity& b) -> bool {
        return a.kind        == b.kind &&
               a.version     == b.version &&
               a.rec_len     == b.rec_len &&
               a.field_count == b.field_count;
    };

    std::string meta_err;
    auto meta = cdxmeta::read_meta(cdx_container_path, &meta_err);

    if (meta.has_value()) {
        if (!cdxmeta::matches(identity, *meta)) {
            const bool core_ok = same_core_shape(identity, meta->table);

            if (core_ok) {
                // Stale or older-style metadata sidecar: refresh it in place.
                cdxmeta::MetaRecord fresh = *meta;
                fresh.table = identity;

                if (!cdxmeta::write_meta(cdx_container_path, fresh, &meta_err)) {
                    if (err) {
                        std::ostringstream oss;
                        oss << "openCdx: stale metadata detected but refresh failed"
                            << " [table kind=" << identity.kind
                            << ", version=" << static_cast<unsigned>(identity.version)
                            << ", reclen=" << identity.rec_len
                            << ", fields=" << identity.field_count
                            << ", hash=" << identity.schema_hash
                            << "]"
                            << " vs"
                            << " [cdx kind=" << meta->table.kind
                            << ", version=" << static_cast<unsigned>(meta->table.version)
                            << ", reclen=" << meta->table.rec_len
                            << ", fields=" << meta->table.field_count
                            << ", hash=" << meta->table.schema_hash
                            << "]"
                            << " write_meta error=" << meta_err;
                        *err = oss.str();
                    }
                    return false;
                }

                // Re-read after refresh so later code sees current metadata.
                meta = cdxmeta::read_meta(cdx_container_path, &meta_err);
                if (!meta.has_value()) {
                    if (err) *err = "openCdx: refreshed metadata but could not re-read sidecar";
                    return false;
                }
            } else {
                if (err) {
                    std::ostringstream oss;
                    oss << "openCdx: metadata mismatch"
                        << " [table kind=" << identity.kind
                        << ", version=" << static_cast<unsigned>(identity.version)
                        << ", reclen=" << identity.rec_len
                        << ", fields=" << identity.field_count
                        << ", hash=" << identity.schema_hash
                        << "]"
                        << " vs"
                        << " [cdx kind=" << meta->table.kind
                        << ", version=" << static_cast<unsigned>(meta->table.version)
                        << ", reclen=" << meta->table.rec_len
                        << ", fields=" << meta->table.field_count
                        << ", hash=" << meta->table.schema_hash
                        << "]";
                    *err = oss.str();
                }
                return false;
            }
        }
    } else {
        // No metadata yet: first-time bind
        cdxmeta::MetaRecord fresh{};
        fresh.table = identity;

        if (!cdxmeta::write_meta(cdx_container_path, fresh, &meta_err)) {
            if (err) *err = "openCdx: failed to write metadata: " + meta_err;
            return false;
        }
    }

    // ---- New architecture: public container -> LMDB backend env ------------
    const fs::path container_path(cdx_container_path);
    const fs::path env_path = dottalk::paths::resolve_lmdb_env_for_cdx(container_path);

    if (!fs::exists(env_path) || !fs::is_directory(env_path)) {
        if (err) {
            *err = "openCdx: LMDB env missing: " + env_path.string();
        }
        return false;
    }

    auto b = std::make_unique<CdxBackend>(area_, cdx_container_path);

    // IMPORTANT:
    // Open the backend against the LMDB env, not the public .cdx container.
    if (!b->open(env_path.string())) {
        if (err) {
            *err = "openCdx: backend open() failed"
                   " [container=" + container_path.string() +
                   ", env=" + env_path.string() + "]";
        }
        return false;
    }

    backend_ = std::move(b);
    container_path_ = cdx_container_path;

    if (!tag_upper.empty()) {
        if (!setTag(tag_upper, err)) return false;
    }
    return true;
}

bool IndexManager::setTag(const std::string& tag_upper, std::string* err) {
    if (!backend_) {
        if (err) *err = "setTag: no active backend";
        return false;
    }
    auto* tb = dynamic_cast<ITagBackend*>(backend_.get());
    if (!tb) {
        if (err) *err = "setTag: backend not tag-capable";
        return false;
    }
    const auto up = to_upper_copy_ascii_(tag_upper);
    tb->setTag(up);
    tag_upper_ = up;
    return true;
}

std::unique_ptr<Cursor> IndexManager::seek(const Key& key) const {
    if (!backend_) return {};
    return backend_->seek(key);
}

std::unique_ptr<Cursor> IndexManager::scan(const Key& low, const Key& high) const {
    if (!backend_) return {};
    return backend_->scan(low, high);
}

bool IndexManager::lmdbSeekUserKey(const std::string& user_key,
                                   std::uint32_t& out_recno,
                                   std::string& out_err) const {
    out_recno = 0;
    out_err.clear();
    auto* cdx = dynamic_cast<const CdxBackend*>(backend_.get());
    if (!cdx) {
        out_err = "no CDX/LMDB backend active";
        return false;
    }
    return cdx->seekRecnoUserKey(user_key, out_recno, out_err);
}

// ---- Canonical active-tag key helpers -------------------------------------

int IndexManager::activeTagFieldIndex1() const {
    const auto tag = activeTag();
    if (tag.empty()) return 0;

    try {
        const auto defs = area_.fields();
        const auto want = to_upper_copy_ascii_(tag);
        for (std::size_t i = 0; i < defs.size(); ++i) {
            if (to_upper_copy_ascii_(defs[i].name) == want) {
                return static_cast<int>(i) + 1; // 1-based
            }
        }
    } catch (...) {
    }
    return 0;
}

bool IndexManager::activeTagMatchesField(int field1) const {
    if (field1 <= 0) return false;
    return activeTagFieldIndex1() == field1;
}

Key IndexManager::buildActiveTagBaseKeyFromString(const std::string& raw_value) const {
    Key out;
    const int fld = activeTagFieldIndex1();
    if (fld <= 0) return out;

    try {
        const auto defs = area_.fields();
        const auto& fdef = defs[static_cast<std::size_t>(fld - 1)];

        std::string s = raw_value;

        const bool is_char = (fdef.type == 'C' || fdef.type == 'c');
        const int keylen = static_cast<int>(fdef.length);

        if (is_char) {
            for (char& c : s) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
        }

        if (keylen > 0) {
            if (static_cast<int>(s.size()) > keylen) {
                s.resize(static_cast<std::size_t>(keylen));
            } else if (static_cast<int>(s.size()) < keylen) {
                s.append(static_cast<std::size_t>(keylen - static_cast<int>(s.size())), ' ');
            }
        }

        out.reserve(s.size());
        out.insert(out.end(), s.begin(), s.end());
    } catch (...) {
        out.clear();
    }

    return out;
}

Key IndexManager::buildActiveTagBaseKeyFromCurrentRecord() const {
    const int fld = activeTagFieldIndex1();
    if (fld <= 0) return {};
    try {
        return buildActiveTagBaseKeyFromString(area_.get(fld));
    } catch (...) {
        return {};
    }
}

// ---- local helpers for field-tag mutation fan-out -------------------------

namespace {

static inline std::string field_name_upper_for_1based_(xbase::DbArea& area, int field1) {
    if (field1 <= 0) return {};
    try {
        const auto defs = area.fields();
        const std::size_t idx = static_cast<std::size_t>(field1 - 1);
        if (idx >= defs.size()) return {};
        std::string s = defs[idx].name;
        for (char& ch : s) {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        }
        return s;
    } catch (...) {
        return {};
    }
}

template<typename Fn>
static bool with_tag_switched_(IndexManager& im,
                               const std::string& want_tag_upper,
                               Fn&& fn)
{
    if (!im.hasBackend()) return false;
    if (want_tag_upper.empty()) return false;

    std::string err;
    const std::string saved_tag = im.activeTag();

    if (!im.setTag(want_tag_upper, &err)) {
        return false;
    }

    bool ok = false;
    try {
        fn();
        ok = true;
    } catch (...) {
        ok = false;
    }

    if (!saved_tag.empty() && saved_tag != want_tag_upper) {
        std::string restore_err;
        (void)im.setTag(saved_tag, &restore_err);
    }

    return ok;
}

} // anonymous namespace

// ---- OO mutation wrappers -------------------------------------------------

bool IndexManager::replace_active_field_value(int field1,
                                              const std::string& old_value,
                                              const std::string& new_value,
                                              RecNo rec) {
    if (!backend_) return false;

    const std::string target_tag = field_name_upper_for_1based_(area_, field1);
    if (target_tag.empty()) return false;

    return with_tag_switched_(*this, target_tag, [&]() {
        const Key old_key = buildActiveTagBaseKeyFromString(old_value);
        const Key new_key = buildActiveTagBaseKeyFromString(new_value);
        on_replace(old_key, new_key, rec);
    });
}

bool IndexManager::append_active_field_value(int field1,
                                             const std::string& value,
                                             RecNo rec) {
    if (!backend_) return false;

    const std::string target_tag = field_name_upper_for_1based_(area_, field1);
    if (target_tag.empty()) return false;

    return with_tag_switched_(*this, target_tag, [&]() {
        const Key key = buildActiveTagBaseKeyFromString(value);
        on_append(key, rec);
    });
}

bool IndexManager::delete_active_field_value(int field1,
                                             const std::string& value,
                                             RecNo rec) {
    if (!backend_) return false;

    const std::string target_tag = field_name_upper_for_1based_(area_, field1);
    if (target_tag.empty()) return false;

    return with_tag_switched_(*this, target_tag, [&]() {
        const Key key = buildActiveTagBaseKeyFromString(value);
        on_delete(key, rec);
    });
}

// ---- legacy shim ----------------------------------------------------------

std::optional<IndexManager::ActiveState> IndexManager::active() const {
    if (!backend_) return std::nullopt;
    ActiveState st;
    st.spec_.cdx = container_path_;
    st.spec_.tag = activeTag();
    return st;
}

bool IndexManager::set_active(const std::string& tagName) {
    std::string err;
    if (!backend_) {
        if (!load_for_table(area_.filename())) {
            return false;
        }
    }
    return setTag(tagName, &err);
}

std::vector<std::string> IndexManager::listTags() const {
    std::vector<std::string> out;
    const auto t = activeTag();
    if (!t.empty()) out.push_back(t);
    return out;
}

static inline fs::path candidate_in_dir_(const fs::path& dir,
                                         const std::string& stem_upper,
                                         const std::string& ext_lower) {
    fs::path p = dir / stem_upper;
    p += ext_lower;
    return p;
}

bool IndexManager::load_for_table(const std::string& path_or_dbf) {
    std::string err;

    if (path_or_dbf.empty()) return false;

    fs::path p(path_or_dbf);
    const auto ext = to_upper_copy_ascii_(p.extension().string());

    if (ext == ".CDX") {
        return openCdx(p.string(), {}, &err);
    }

    // If a DBF was provided, resolve likely .cdx locations.
    if (ext == ".DBF" || ext.empty()) {
        fs::path dbf = p;
        if (ext.empty()) {
            dbf = fs::path(area_.dbfDir()) / p;
            dbf += ".dbf";
        }

        const auto stem_upper = to_upper_copy_ascii_(dbf.stem().string());

        std::vector<fs::path> cand;
        if (!dbf.parent_path().empty()) {
            cand.push_back(candidate_in_dir_(dbf.parent_path(), stem_upper, ".cdx"));
            cand.push_back(candidate_in_dir_(dbf.parent_path() / "indexes", stem_upper, ".cdx"));
        }
        cand.push_back(candidate_in_dir_(fs::path("indexes"), stem_upper, ".cdx"));
        if (!area_.dbfDir().empty()) {
            cand.push_back(candidate_in_dir_(fs::path(area_.dbfDir()), to_upper_copy_ascii_(area_.dbfBasename()), ".cdx"));
            cand.push_back(candidate_in_dir_(fs::path(area_.dbfDir()) / "indexes", to_upper_copy_ascii_(area_.dbfBasename()), ".cdx"));
        }

        for (const auto& c : cand) {
            if (fs::exists(c)) return openCdx(c.string(), {}, &err);
        }

        if (!cand.empty()) return openCdx(cand.front().string(), {}, &err);
        return false;
    }

    return openCdx(p.string(), {}, &err);
}

bool IndexManager::load_json(const std::string& inx_path) {
    std::string err;
    if (inx_path.empty()) return false;

    fs::path p(inx_path);
    const auto doc = read_all_text_(p);
    if (doc.empty()) {
        return false;
    }

    std::string cdx = json_string_value_(doc, "cdx");
    std::string tag = json_string_value_(doc, "tag");

    if (cdx.empty()) {
        cdx = (p.parent_path() / p.stem()).string() + ".cdx";
    }
    if (tag.empty()) {
        tag = p.stem().string();
    }

    return openCdx(cdx, to_upper_copy_ascii_(tag), &err);
}

} // namespace xindex
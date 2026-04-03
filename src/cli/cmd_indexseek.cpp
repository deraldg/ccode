// src/cli/cmd_indexseek.cpp
//
// INDEXSEEK <value> [SOFT] [TAG <path-or-name>]
//
// Returns a record number without moving the current record pointer.
//
// Supported behavior:
// - Active CDX/LMDB order: use area-local IndexManager / CDX backend.
// - TAG <name>: if current area has a CDX attached, use that tag from the same container.
// - TAG <file.cdx|path\file.cdx|file.cdx.d>: open that CDX/LMDB container and use the
//   requested/current tag when possible.
// - Legacy INX fallback remains supported ("1INX" / "2INX").
//
// Output:
//   INDEXSEEK(): <recno>
//
// Behavior:
// - Exact hit => matching recno
// - Miss => 0, unless SOFT, where first >= key (by ordered scan) is returned.
//
// Notes:
// - For CDX/LMDB, this implementation uses the public cursor API actually present
//   in the tree: first/last/next/prev.
// - It does not move the caller's record pointer.

#include "xbase.hpp"
#include "xindex/index_manager.hpp"
#include "cli/path_resolver.hpp"
#include "cli/order_state.hpp"
#include "textio.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace {

struct CursorRestore {
    xbase::DbArea* a{nullptr};
    int32_t saved{0};
    bool active{false};

    explicit CursorRestore(xbase::DbArea& area) : a(&area) {
        try {
            saved = area.recno();
            active = (saved >= 1 && saved <= area.recCount());
        } catch (...) {
            active = false;
        }
    }

    ~CursorRestore() {
        if (!active || !a) return;
        try {
            if (a->gotoRec(saved)) {
                (void)a->readCurrent();
            }
        } catch (...) {
        }
    }

    CursorRestore(const CursorRestore&) = delete;
    CursorRestore& operator=(const CursorRestore&) = delete;
};

static bool rd_u16(std::ifstream& in, uint16_t& v) {
    unsigned char b[2];
    if (!in.read(reinterpret_cast<char*>(b), 2)) return false;
    v = static_cast<uint16_t>(b[0] | (static_cast<uint16_t>(b[1]) << 8));
    return true;
}

static bool rd_u32(std::ifstream& in, uint32_t& v) {
    unsigned char b[4];
    if (!in.read(reinterpret_cast<char*>(b), 4)) return false;
    v = static_cast<uint32_t>(b[0]
        | (static_cast<uint32_t>(b[1]) << 8)
        | (static_cast<uint32_t>(b[2]) << 16)
        | (static_cast<uint32_t>(b[3]) << 24));
    return true;
}

static std::string trim_copy(const std::string& s) {
    return textio::trim(s);
}

static std::string upper_copy(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

static std::string strip_outer_quotes(std::string s) {
    s = trim_copy(s);
    if (s.size() >= 2) {
        const char a = s.front();
        const char b = s.back();
        if ((a == '"' && b == '"') || (a == '\'' && b == '\'')) {
            s = s.substr(1, s.size() - 2);
        }
    }
    return s;
}

static std::string norm_seek_text(std::string s) {
    return upper_copy(trim_copy(s));
}

static bool ieq_char(char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) ==
           std::tolower(static_cast<unsigned char>(b));
}

static bool ieq(const std::string& a, const char* b) {
    if (!b) return false;
    size_t m = 0;
    while (b[m] != '\0') ++m;
    if (a.size() != m) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (!ieq_char(a[i], b[i])) return false;
    }
    return true;
}

static bool ends_with_ci(const std::string& s, const std::string& suf) {
    if (s.size() < suf.size()) return false;
    const size_t off = s.size() - suf.size();
    for (size_t i = 0; i < suf.size(); ++i) {
        if (!ieq_char(s[off + i], suf[i])) return false;
    }
    return true;
}

static int find_field_1based(const xbase::DbArea& a, const std::string& name_ci) {
    const auto& fds = a.fields();
    for (size_t i = 0; i < fds.size(); ++i) {
        if (ieq(fds[i].name, name_ci.c_str())) {
            return static_cast<int>(i) + 1;
        }
    }
    return 0;
}

static bool field_value_matches(xbase::DbArea& a, int fld, const std::string& needleU) {
    const std::string curU = norm_seek_text(a.get(fld));
    return curU == needleU;
}

struct ParsedArgs {
    std::string value;
    bool soft{false};
    fs::path tagArg;
};

static bool parse_value_and_flags(const std::string& rest, ParsedArgs& out) {
    out = ParsedArgs{};

    std::istringstream in(rest);
    in >> std::ws;
    if (!in.good()) return false;

    if (in.peek() == '"' || in.peek() == '\'') {
        const char q = static_cast<char>(in.get());
        std::getline(in, out.value, q);
    } else {
        if (!(in >> out.value)) return false;
    }

    std::string tok;
    while (in >> tok) {
        const std::string up = upper_copy(tok);
        if (up == "SOFT") {
            out.soft = true;
        } else if (up == "TAG") {
            std::string p;
            if (!(in >> p)) return false;
            out.tagArg = fs::path(p);
        }
    }
    return true;
}

// ------------------------------------------------------------
// Legacy INX support
// ------------------------------------------------------------

struct InxMeta {
    enum class Kind { Unknown, Inx1, Inx2 };
    Kind kind{Kind::Unknown};
    uint16_t version{0};
    uint16_t keylen{0};
    std::string exprTok;
    uint32_t count{0};
    uint32_t recCountDbf{0};
    std::uint64_t entries_off{0};
};

static bool read_meta(const fs::path& p, InxMeta& m) {
    m = InxMeta{};
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;

    char magic[4]{};
    if (!in.read(magic, 4)) return false;

    if (std::memcmp(magic, "2INX", 4) == 0) {
        m.kind = InxMeta::Kind::Inx2;
        if (!rd_u16(in, m.version)) return false;
        if (!rd_u16(in, m.keylen)) return false;
        uint16_t exprLen{};
        if (!rd_u16(in, exprLen)) return false;
        m.exprTok.resize(exprLen);
        if (!in.read(m.exprTok.data(), static_cast<std::streamsize>(exprLen))) return false;
        if (!rd_u32(in, m.count)) return false;
        if (!rd_u32(in, m.recCountDbf)) return false;
        m.entries_off = static_cast<std::uint64_t>(in.tellg());
        return true;
    }

    if (std::memcmp(magic, "1INX", 4) == 0) {
        m.kind = InxMeta::Kind::Inx1;
        if (!rd_u16(in, m.version)) return false;
        uint16_t exprLen{};
        if (!rd_u16(in, exprLen)) return false;
        m.exprTok.resize(exprLen);
        if (!in.read(m.exprTok.data(), static_cast<std::streamsize>(exprLen))) return false;
        if (!rd_u32(in, m.count)) return false;
        m.entries_off = static_cast<std::uint64_t>(in.tellg());
        return true;
    }

    return false;
}

static bool inx2_lower_bound_recno(const fs::path& p,
                                   const InxMeta& m,
                                   const std::string& needle_fixed,
                                   uint32_t& out_recno,
                                   bool& out_exact) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;

    const std::uint64_t entry_sz = static_cast<std::uint64_t>(m.keylen) + 4ULL;

    auto read_key_recno = [&](uint32_t pos, std::string& keyOut, uint32_t& rnOut) -> bool {
        const std::uint64_t off = m.entries_off + entry_sz * static_cast<std::uint64_t>(pos);
        in.seekg(static_cast<std::streamoff>(off), std::ios::beg);
        keyOut.resize(m.keylen);
        if (!in.read(keyOut.data(), static_cast<std::streamsize>(m.keylen))) return false;
        return rd_u32(in, rnOut);
    };

    uint32_t lo = 0, hi = m.count;
    std::string key;
    uint32_t rn{};

    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2;
        if (!read_key_recno(mid, key, rn)) return false;
        if (key < needle_fixed) lo = mid + 1;
        else hi = mid;
    }

    out_exact = false;
    if (lo >= m.count) return false;
    if (!read_key_recno(lo, key, rn)) return false;

    out_recno = rn;
    if (key == needle_fixed) out_exact = true;
    return true;
}

static bool inx1_lower_bound_recno_stream(const fs::path& p,
                                          const InxMeta& m,
                                          const std::string& needle_norm,
                                          uint32_t& out_recno,
                                          bool& out_exact) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;

    in.seekg(static_cast<std::streamoff>(m.entries_off), std::ios::beg);

    out_exact = false;
    out_recno = 0;

    for (uint32_t i = 0; i < m.count; ++i) {
        uint16_t klen{};
        if (!rd_u16(in, klen)) return false;

        std::string key;
        key.resize(klen);
        if (!in.read(key.data(), static_cast<std::streamsize>(klen))) return false;

        uint32_t rn{};
        if (!rd_u32(in, rn)) return false;

        const std::string k2 = upper_copy(trim_copy(key));
        if (k2 < needle_norm) continue;

        out_recno = rn;
        if (k2 == needle_norm) out_exact = true;
        return true;
    }

    return false;
}

static bool indexseek_via_inx(const fs::path& tagPath,
                              const std::string& value,
                              bool soft,
                              uint32_t& out_recno) {
    out_recno = 0;

    InxMeta m{};
    if (!read_meta(tagPath, m) || m.count == 0) {
        return false;
    }

    bool exact = false;
    uint32_t recno = 0;

    if (m.kind == InxMeta::Kind::Inx2) {
        std::string needle = upper_copy(trim_copy(value));
        if (needle.size() > m.keylen) needle.resize(m.keylen);
        if (needle.size() < m.keylen) {
            needle.append(static_cast<size_t>(m.keylen - needle.size()), ' ');
        }

        if (!inx2_lower_bound_recno(tagPath, m, needle, recno, exact)) {
            return false;
        }
    } else if (m.kind == InxMeta::Kind::Inx1) {
        const std::string needle_norm = upper_copy(trim_copy(value));
        if (!inx1_lower_bound_recno_stream(tagPath, m, needle_norm, recno, exact)) {
            return false;
        }
    } else {
        return false;
    }

    if (!exact && !soft) return false;

    out_recno = recno;
    return true;
}

// ------------------------------------------------------------
// CDX / LMDB support
// ------------------------------------------------------------

struct CdxResolve {
    std::string containerPath;
    std::string tagName;
    bool ok{false};
};

static CdxResolve resolve_cdx_target(xbase::DbArea& A, const fs::path& tagArg) {
    CdxResolve out{};

    const std::string activeContainer = orderstate::orderName(A);
    const std::string activeTag = orderstate::activeTag(A);

    if (tagArg.empty()) {
        if (orderstate::hasOrder(A) && orderstate::isCdx(A) &&
            !activeContainer.empty() && !activeTag.empty()) {
            out.containerPath = activeContainer;
            out.tagName = upper_copy(activeTag);
            out.ok = true;
        }
        return out;
    }

    const std::string raw = tagArg.string();
    const std::string rawU = upper_copy(raw);

    if (ends_with_ci(rawU, ".CDX") || ends_with_ci(rawU, ".CDX.D")) {
        fs::path p = tagArg;
        if (ends_with_ci(rawU, ".CDX.D")) {
            std::string s = p.string();
            s.resize(s.size() - 2); // drop trailing ".d"
            p = fs::path(s);
        }

        out.containerPath = dottalk::paths::resolve_index(p.string()).string();
        if (!activeTag.empty()) out.tagName = upper_copy(activeTag);
        out.ok = !out.containerPath.empty() && !out.tagName.empty();
        return out;
    }

    if (orderstate::hasOrder(A) && orderstate::isCdx(A) && !activeContainer.empty()) {
        out.containerPath = activeContainer;
        out.tagName = upper_copy(raw);
        out.ok = true;
        return out;
    }

    return out;
}

static bool indexseek_via_cdx(xbase::DbArea& A,
                              const std::string& value,
                              bool soft,
                              const fs::path& tagArg,
                              uint32_t& out_recno) {
    out_recno = 0;

    CdxResolve target = resolve_cdx_target(A, tagArg);
    if (!target.ok) return false;

    // Current dev contract: tag name == field name.
    const int fld = find_field_1based(A, target.tagName);
    if (fld <= 0) return false;

    const std::string needleU = norm_seek_text(strip_outer_quotes(value));

    auto& im = A.indexManager();
    std::string err;

    if (!im.hasBackend() || !im.isCdx() || im.containerPath() != target.containerPath) {
        if (!im.openCdx(target.containerPath, target.tagName, &err)) {
            return false;
        }
    } else {
        if (!im.setTag(target.tagName, &err)) {
            return false;
        }
    }

    if (!im.hasBackend() || !im.isCdx() || im.activeTag().empty()) {
        return false;
    }

    auto cur = im.scan(xindex::Key{}, xindex::Key{});
    if (!cur) return false;

    const bool asc = orderstate::isAscending(A);

    xindex::Key k;
    xindex::RecNo r;
    bool ok = asc ? cur->first(k, r) : cur->last(k, r);

    while (ok) {
        const int32_t rn = static_cast<int32_t>(r);
        if (rn > 0 && rn <= A.recCount()) {
            try {
                if (A.gotoRec(rn) && A.readCurrent()) {
                    if (!A.isDeleted()) {
                        const std::string curU = norm_seek_text(A.get(fld));

                        if (curU == needleU) {
                            out_recno = static_cast<uint32_t>(rn);
                            return true;
                        }

                        if (soft && curU >= needleU) {
                            out_recno = static_cast<uint32_t>(rn);
                            return true;
                        }
                    }
                }
            } catch (...) {
                return false;
            }
        }

        ok = asc ? cur->next(k, r) : cur->prev(k, r);
    }

    return false;
}

} // anonymous namespace

void cmd_INDEXSEEK(xbase::DbArea& A, std::istringstream& args) {
    if (!A.isOpen()) {
        std::cout << "INDEXSEEK(): 0\n";
        return;
    }

    CursorRestore restore(A);

    std::string rest;
    std::getline(args, rest);
    rest = trim_copy(rest);

    ParsedArgs parsed;
    if (!parse_value_and_flags(rest, parsed)) {
        std::cout << "INDEXSEEK(): 0\n";
        return;
    }

    // Prefer active CDX/LMDB path when appropriate.
    {
        const std::string tagRaw = parsed.tagArg.string();
        const std::string tagU = upper_copy(tagRaw);

        const bool tagLooksCdx =
            ends_with_ci(tagU, ".CDX") ||
            ends_with_ci(tagU, ".CDX.D");

        const bool preferCdx =
            (orderstate::hasOrder(A) && orderstate::isCdx(A)) ||
            (!parsed.tagArg.empty() && tagLooksCdx);

        if (preferCdx) {
            uint32_t recno = 0;
            if (indexseek_via_cdx(A, parsed.value, parsed.soft, parsed.tagArg, recno)) {
                std::cout << "INDEXSEEK(): " << recno << "\n";
                return;
            }
        }
    }

    // Legacy INX fallback.
    {
        fs::path tagPath;
        if (!parsed.tagArg.empty()) {
            tagPath = dottalk::paths::resolve_index(parsed.tagArg.string());
        } else {
            fs::path base(A.name());
            std::string stem = base.stem().string();
            if (stem.empty()) stem = "table";
            tagPath = dottalk::paths::resolve_index(stem + ".inx");
        }
        if (!tagPath.has_extension()) tagPath.replace_extension(".inx");

        uint32_t recno = 0;
        if (indexseek_via_inx(tagPath, parsed.value, parsed.soft, recno)) {
            std::cout << "INDEXSEEK(): " << recno << "\n";
            return;
        }
    }

    std::cout << "INDEXSEEK(): 0\n";
}
#include "xindex/index_manager.hpp"
#include "xindex/index_spec.hpp"
#include "xindex/index_key.hpp"
#include <fstream>
#include <sstream>
#include <cctype>

// Project headers expected in ccode:
#include "xbase.hpp"
#include "textio.hpp"

namespace xindex {

using std::string;
using std::vector;

static bool ieq(std::string a, std::string b) {
    if (a.size()!=b.size()) return false;
    for (size_t i=0;i<a.size();++i)
        if (std::toupper((unsigned char)a[i]) != std::toupper((unsigned char)b[i])) return false;
    return true;
}

std::string IndexManager::trim(std::string s) {
    auto issp = [](unsigned char c){ return std::isspace(c)!=0; };
    while (!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && issp((unsigned char)s.back())) s.pop_back();
    return s;
}
std::string IndexManager::up(std::string s) {
    for (auto& ch : s) ch = static_cast<char>(std::toupper((unsigned char)ch));
    return s;
}

IndexManager::IndexManager(xbase::DbArea& area) : area_(area) {}

bool IndexManager::load_for_table(std::string const& dbfPath) {
    auto p = inx_path(dbfPath);
    return load_json(p);
}

bool IndexManager::save(std::string const& dbfPath) {
    if (!dirty_) return true;
    auto p = inx_path(dbfPath);
    return save_json(p);
}

IndexTag* IndexManager::active() {
    auto it = tags_.find(active_tag_);
    if (it == tags_.end()) return nullptr;
    return it->second.get();
}
const IndexTag* IndexManager::active() const {
    auto it = tags_.find(active_tag_);
    if (it == tags_.end()) return nullptr;
    return it->second.get();
}
bool IndexManager::has_active() const {
    return !active_tag_.empty() && tags_.count(active_tag_)>0;
}

bool IndexManager::set_active(std::string const& tag) {
    if (tags_.count(tag)==0) return false;
    active_tag_ = tag;
    dir_ascending_ = tags_[tag]->spec().ascending;
    return true;
}
void IndexManager::clear_active() { active_tag_.clear(); }

void IndexManager::set_direction(bool ascending) { dir_ascending_ = ascending; }

std::string IndexManager::inx_path(std::string const& dbfPath) const {
    auto pos = dbfPath.find_last_of('.');
    if (pos == std::string::npos) return dbfPath + ".inx";
    return dbfPath.substr(0, pos) + ".inx";
}

IndexKey IndexManager::make_key_from_tokens(IndexSpec const& spec, const std::vector<std::string>& toks) const {
    IndexKey k; k.parts.reserve(spec.fields.size());
    for (size_t i=0; i<spec.fields.size(); ++i) {
        std::string v = (i < toks.size() ? trim(toks[i]) : std::string());
        bool isnum = !v.empty() && (std::isdigit((unsigned char)v[0]) || v[0]=='-' || v[0]=='+');
        if (isnum) {
            char* endp=nullptr; double d = std::strtod(v.c_str(), &endp);
            if (endp && *endp=='\0') { k.parts.emplace_back(d); continue; }
        }
        k.parts.emplace_back(up(v)); // treat as string
    }
    return k;
}

IndexKey IndexManager::key_from_record(IndexSpec const& spec, int recno) const {
    IndexKey k; k.parts.reserve(spec.fields.size());
    for (auto const& fname : spec.fields) {
        std::string sv = area_.get_field_as_string(recno, fname);
        if (!sv.empty()) { k.parts.emplace_back(up(trim(sv))); }
        else { double nv = area_.get_field_as_double(recno, fname); k.parts.emplace_back(nv); }
    }
    return k;
}

IndexTag& IndexManager::ensure_tag(IndexSpec const& specIn) {
    IndexSpec spec = specIn;
    if (spec.tag.empty()) spec.tag = spec.fields.empty()? std::string("TAG") : spec.fields.front();
    auto it = tags_.find(spec.tag);
    if (it != tags_.end()) {
        return *it->second;
    }
    auto tag = std::make_unique<IndexTag>(spec);
    std::vector<KeyRec> rows;
    int n = area_.record_count();
    rows.reserve(n>0?n:0);
    for (int rec=1; rec<=n; ++rec) {
        if (area_.is_deleted(rec)) continue;
        IndexKey k = key_from_record(spec, rec);
        rows.push_back(KeyRec{ std::move(k), rec });
    }
    tag->bulk_build(std::move(rows));
    auto& ref = *tag;
    tags_[spec.tag] = std::move(tag);
    dirty_ = true;
    return ref;
}

int IndexManager::top() const { auto a = active(); return a ? (dir_ascending_? a->top() : a->bottom()) : -1; }
int IndexManager::bottom() const { auto a = active(); return a ? (dir_ascending_? a->bottom() : a->top()) : -1; }

void IndexManager::on_append(int recno) {
    for (auto& kv : tags_) {
        auto& tag = *kv.second;
        IndexKey k = key_from_record(tag.spec(), recno);
        tag.insert(std::move(k), recno);
    }
    dirty_ = true;
}
void IndexManager::on_replace(int recno) {
    for (auto& kv : tags_) {
        kv.second->erase_recno(recno);
        IndexKey k = key_from_record(kv.second->spec(), recno);
        kv.second->insert(std::move(k), recno);
    }
    dirty_ = true;
}
void IndexManager::on_delete(int recno) {
    for (auto& kv : tags_) kv.second->erase_recno(recno);
    dirty_ = true;
}
void IndexManager::on_recall(int recno) {
    for (auto& kv : tags_) {
        IndexKey k = key_from_record(kv.second->spec(), recno);
        kv.second->insert(std::move(k), recno);
    }
    dirty_ = true;
}
void IndexManager::on_pack(std::function<int(int)> recnoRemap) {
    std::unordered_map<std::string, std::unique_ptr<IndexTag>> rebuilt;
    for (auto& kv : tags_) {
        auto spec = kv.second->spec();
        auto tag = std::make_unique<IndexTag>(spec);
        std::vector<KeyRec> rows;
        int n = area_.record_count();
        rows.reserve(n>0?n:0);
        for (int rec=1; rec<=n; ++rec) {
            if (area_.is_deleted(rec)) continue;
            IndexKey k = key_from_record(spec, rec);
            int newRec = recnoRemap ? recnoRemap(rec) : rec;
            rows.push_back(KeyRec{ std::move(k), newRec });
        }
        tag->bulk_build(std::move(rows));
        rebuilt[spec.tag] = std::move(tag);
    }
    tags_ = std::move(rebuilt);
    dirty_ = true;
}
void IndexManager::on_zap() {
    tags_.clear();
    active_tag_.clear();
    dirty_ = true;
}

// --- Persistence (simple JSON for our own output) ---
static void write_json_string(std::ostream& os, const std::string& s) {
    os << '"';
    for (char c : s) {
        if (c=='"' || c=='\\') { os << '\\' << c; }
        else if (c=='\n') { os << "\\n"; }
        else { os << c; }
    }
    os << '"';
}

bool IndexManager::save_json(std::string const& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "{";
    out << ""version":1,";
    out << ""tags":[";
    bool firstTag = true;
    for (auto& kv : tags_) {
        auto& tag = *kv.second;
        if (!firstTag) out << ","; firstTag = false;
        out << "{";
        out << ""tag":"; write_json_string(out, tag.spec().tag); out << ",";
        out << ""ascending":" << (tag.spec().ascending ? "true":"false") << ",";
        out << ""unique":" << (tag.spec().unique ? "true":"false") << ",";
        out << ""fields":[";
        for (size_t i=0;i<tag.spec().fields.size();++i) {
            if (i) out << ",";
            write_json_string(out, tag.spec().fields[i]);
        }
        out << "],"entries":[";
        bool firstEnt = true;
        for (auto const& kr : tag.entries()) {
            if (!firstEnt) out << ","; firstEnt = false;
            out << "{"k":[";
            for (size_t i=0;i<kr.key.parts.size();++i) {
                if (i) out << ",";
                if (std::holds_alternative<std::string>(kr.key.parts[i])) {
                    write_json_string(out, std::get<std::string>(kr.key.parts[i]));
                } else {
                    char buf[64]; std::snprintf(buf, sizeof(buf), "%.15g", std::get<double>(kr.key.parts[i]));
                    write_json_string(out, std::string(buf));
                }
            }
            out << "],"r":" << kr.recno << "}";
        }
        out << "]}";
    }
    out << "]";
    out << "}";
    return true;
}

static std::string read_all(std::istream& in) { std::ostringstream ss; ss << in.rdbuf(); return ss.str(); }

bool IndexManager::load_json(std::string const& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::string s = read_all(in);
    size_t posTags = s.find(""tags"");
    if (posTags==std::string::npos) return false;
    size_t posArr = s.find("[", posTags);
    if (posArr==std::string::npos) return false;
    size_t pos = posArr+1;
    tags_.clear();

    while (true) {
        size_t o = s.find("{", pos);
        if (o==std::string::npos) break;
        size_t depth=0, j=o;
        for (; j<s.size(); ++j) {
            if (s[j]=='{') depth++;
            else if (s[j]=='}') { depth--; if (depth==0) { ++j; break; } }
        }
        if (j<=o) break;
        std::string obj = s.substr(o, j-o);

        auto get_str = [&](const char* key)->std::string{
            std::string pat = std::string(""")+key+""";
            size_t p = obj.find(pat);
            if (p==std::string::npos) return {};
            p = obj.find(':', p); if (p==std::string::npos) return {};
            p = obj.find('"', p); if (p==std::string::npos) return {};
            size_t q = obj.find('"', p+1); if (q==std::string::npos) return {};
            return obj.substr(p+1, q-p-1);
        };
        auto get_bool = [&](const char* key)->bool{
            std::string pat = std::string(""")+key+""";
            size_t p = obj.find(pat); if (p==std::string::npos) return true;
            p = obj.find(':', p); if (p==std::string::npos) return true;
            size_t q = obj.find_first_not_of(" 	", p+1);
            return obj.compare(q, 4, "true")==0;
        };
        auto get_fields = [&]()->std::vector<std::string>{
            std::vector<std::string> out;
            size_t p = obj.find(""fields"");
            if (p==std::string::npos) return out;
            p = obj.find('[', p); if (p==std::string::npos) return out;
            size_t q = obj.find(']', p); if (q==std::string::npos) return out;
            std::string arr = obj.substr(p+1, q-p-1);
            size_t i=0;
            while (true) {
                size_t a = arr.find('"', i); if (a==std::string::npos) break;
                size_t b = arr.find('"', a+1); if (b==std::string::npos) break;
                out.push_back(arr.substr(a+1, b-a-1));
                i = b+1;
            }
            return out;
        };
        auto get_entries = [&]()->std::vector<KeyRec>{
            std::vector<KeyRec> out;
            size_t p = obj.find(""entries"");
            if (p==std::string::npos) return out;
            p = obj.find('[', p); if (p==std::string::npos) return out;
            size_t q = obj.find(']', p); if (q==std::string::npos) return out;
            std::string arr = obj.substr(p+1, q-p-1);
            size_t i = 0;
            while (true) {
                size_t eo = arr.find('{', i);
                if (eo==std::string::npos) break;
                size_t depth=0, k=eo;
                for (; k<arr.size(); ++k) {
                    if (arr[k]=='{') depth++;
                    else if (arr[k]=='}') { depth--; if (depth==0) { ++k; break; } }
                }
                if (k<=eo) break;
                std::string ent = arr.substr(eo, k-eo);
                KeyRec kr;
                size_t pk = ent.find(""k""); if (pk!=std::string::npos) {
                    pk = ent.find('[', pk); size_t qk = ent.find(']', pk);
                    if (qk!=std::string::npos) {
                        std::string arrk = ent.substr(pk+1, qk-pk-1);
                        size_t u=0;
                        while (true) {
                            size_t a = arrk.find('"', u); if (a==std::string::npos) break;
                            size_t b = arrk.find('"', a+1); if (b==std::string::npos) break;
                            kr.key.parts.emplace_back(arrk.substr(a+1, b-a-1));
                            u = b+1;
                        }
                    }
                }
                size_t pr = ent.find(""r"");
                if (pr!=std::string::npos) {
                    pr = ent.find(':', pr); if (pr!=std::string::npos) {
                        kr.recno = std::atoi(ent.c_str() + pr + 1);
                    }
                }
                out.push_back(std::move(kr));
                i = k;
            }
            return out;
        };

        IndexSpec spec;
        spec.tag = get_str("tag");
        spec.ascending = get_bool("ascending");
        spec.unique = get_bool("unique");
        spec.fields = get_fields();
        auto tag = std::make_unique<IndexTag>(spec);
        tag->bulk_build(get_entries());
        tags_[spec.tag] = std::move(tag);

        pos = j;
        size_t nextComma = s.find(',', pos);
        size_t nextBrace = s.find(']', pos);
        if (nextBrace != std::string::npos && (nextComma==std::string::npos || nextBrace < nextComma)) break;
    }
    return !tags_.empty();
}

} // namespace xindex


// src/cli/relations_boot.cpp
// Auto-load relations on startup; auto-save on process exit.
// Honors env toggles: SB_REL_AUTOLOAD=0 or SB_REL_AUTOSAVE=0 to disable.

#include "relations_boot.hpp"
#include "set_relations.hpp"

#include <cstdlib>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

namespace {

static std::string default_rel_path() { return std::string(".relations/relations.json"); }

static bool env_enabled(const char* name, bool defval) {
    const char* v = std::getenv(name);
    if (!v) return defval;
    if (*v=='0') return false;
    return true;
}

static void load_default_if_exists() {
    const std::string path = default_rel_path();
    std::ifstream f(path, std::ios::binary);
    if (!f) return;
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    // Tiny tolerant parser for the format we emit
    std::vector<relations_api::RelationSpec> specs;
    size_t pos = data.find("[", data.find("\"relations\""));
    if (pos == std::string::npos) return;
    while (true) {
        size_t p_obj = data.find("{", pos);
        size_t p_end = data.find("]", pos);
        if (p_obj == std::string::npos || (p_end != std::string::npos && p_obj > p_end)) break;
        size_t p_obj_end = data.find("}", p_obj);
        if (p_obj_end == std::string::npos) break;
        std::string obj = data.substr(p_obj, p_obj_end - p_obj + 1);
        auto trim = [](std::string s){ auto sp=[](unsigned char c){return c==' '||c=='\t'||c=='\r'||c=='\n';};
            while(!s.empty()&&sp((unsigned char)s.front())) s.erase(s.begin());
            while(!s.empty()&&sp((unsigned char)s.back())) s.pop_back();
            return s; };
        auto grab_string = [&](const std::string& key)->std::string{
            std::string pat = "\"" + key + "\":\"";
            size_t s = obj.find(pat);
            if (s == std::string::npos) return "";
            s += pat.size();
            size_t e = obj.find("\"", s);
            if (e == std::string::npos) return "";
            return obj.substr(s, e - s);
        };
        auto grab_fields = [&]()->std::vector<std::string>{
            std::vector<std::string> out;
            size_t s = obj.find("\"fields\":[");
            if (s == std::string::npos) return out;
            s += 10;
            size_t e = obj.find("]", s);
            if (e == std::string::npos) return out;
            std::string arr = obj.substr(s, e - s);
            std::istringstream as(arr);
            std::string tok;
            while (std::getline(as, tok, ',')) {
                tok = trim(tok);
                if (!tok.empty() && tok.front()=='\"' && tok.back()=='\"') {
                    tok = tok.substr(1, tok.size()-2);
                }
                if (!tok.empty()) out.push_back(tok);
            }
            return out;
        };
        relations_api::RelationSpec rs;
        rs.parent = grab_string("parent");
        rs.child  = grab_string("child");
        rs.fields = grab_fields();
        if (!rs.parent.empty() && !rs.child.empty() && !rs.fields.empty()) specs.push_back(std::move(rs));
        pos = p_obj_end + 1;
    }
    relations_api::import_relations(specs, /*clear_existing*/true);
}

static void save_default() {
    const std::string path = default_rel_path();
    // ensure dir exists (simple approach)
#ifdef _WIN32
    _wmkdir(L".relations");
#else
    ::system("mkdir -p .relations");
#endif
    auto specs = relations_api::export_relations();
    std::ofstream f(path, std::ios::binary);
    if (!f) return;
    auto jesc = [](const std::string& s){
        std::string out; out.reserve(s.size()+8);
        for (char c : s) {
            switch (c) {
                case '\\': out += "\\\\"; break;
                case '"':  out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out.push_back(c); break;
            }
        }
        return out;
    };
    f << "{\n  \"relations\": [\n";
    for (size_t i=0;i<specs.size();++i) {
        const auto& s = specs[i];
        f << "    {\"parent\":\"" << jesc(s.parent) << "\",\"child\":\"" << jesc(s.child) << "\",\"fields\":[";
        for (size_t j=0;j<s.fields.size();++j) {
            if (j) f << ",";
            f << "\"" << jesc(s.fields[j]) << "\"";
        }
        f << "]}";
        if (i + 1 < specs.size()) f << ",";
        f << "\n";
    }
    f << "  ]\n}\n";
}

struct AutoBoot {
    AutoBoot() {
        if (env_enabled("SB_REL_AUTOLOAD", true)) load_default_if_exists();
        if (env_enabled("SB_REL_AUTOSAVE", true)) std::atexit(save_default);
    }
} g_autoboot;

} // namespace

namespace relations_boot {
void autoload() { load_default_if_exists(); }
void autosave() { save_default(); }
} // namespace relations_boot

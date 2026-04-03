#include "workspace/schema_workspace.hpp"
#include "workspace/workarea_manager.hpp"
#include "xbase.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

#include "cli/order_state.hpp"

// Engine bridge
extern "C" xbase::XBaseEngine* shell_engine();

namespace dottalk::workspace {

namespace {

static inline std::string trim_copy(std::string s)
{
    auto is_space = [](unsigned char ch){ return std::isspace(ch) != 0; };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
        [&](unsigned char c){ return !is_space(c); }));
    s.erase(std::find_if(s.rbegin(), s.rend(),
        [&](unsigned char c){ return !is_space(c); }).base(), s.end());
    return s;
}

static inline bool ci_equal(const std::string& a, const std::string& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) {
            return false;
        }
    }
    return true;
}

static inline std::string infer_index_type_from_path(const std::string& path)
{
    if (path.empty() || ci_equal(path, "none")) return "NONE";

    auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return "UNKNOWN";

    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c){ return (char)std::tolower(c); });

    if (ext == ".inx") return "INX";
    if (ext == ".cnx") return "CNX";
    if (ext == ".cdx") return "CDX";
    return "UNKNOWN";
}

template <typename Area>
static inline std::string get_order_name_safe(Area& a)
{
    try { return orderstate::orderName(a); } catch (...) { return {}; }
}

template <typename Area>
static inline std::string get_active_tag_safe(Area& a)
{
    try { return orderstate::activeTag(a); } catch (...) {}
    return {};
}

template <typename Area>
static inline bool set_active_tag_safe(Area& a, const std::string& tag)
{
    if (tag.empty() || ci_equal(tag, "none")) return true;

    try {
        orderstate::setActiveTag(a, tag);
        return true;
    } catch (...) {}

    return false;
}

template <typename T>
using has_setLogicalName_t = decltype(std::declval<T&>().setLogicalName(std::declval<std::string>()));
template <typename T, typename = has_setLogicalName_t<T>>
static inline void setLogicalNameIf(T& a, const std::string& s, int) { a.setLogicalName(s); }
template <typename T>
static inline void setLogicalNameIf(T&, const std::string&, long) {}

template <typename T>
using has_setName_t = decltype(std::declval<T&>().setName(std::declval<std::string>()));
template <typename T, typename = has_setName_t<T>>
static inline void setLegacyNameIf(T& a, const std::string& s, int) { a.setName(s); }
template <typename T>
static inline void setLegacyNameIf(T&, const std::string&, long) {}

static inline std::string encode_field(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);

    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '|':  out += "\\p";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            default:   out.push_back(c); break;
        }
    }
    return out;
}

static inline std::string decode_field(const std::string& s)
{
    std::string out;
    out.reserve(s.size());

    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '\\' || i + 1 >= s.size()) {
            out.push_back(s[i]);
            continue;
        }

        const char n = s[++i];
        switch (n) {
            case '\\': out.push_back('\\'); break;
            case 'p':  out.push_back('|');  break;
            case 'n':  out.push_back('\n'); break;
            case 'r':  out.push_back('\r'); break;
            default:
                out.push_back('\\');
                out.push_back(n);
                break;
        }
    }

    return out;
}

static inline std::vector<std::string> split_pipe_escaped(const std::string& s)
{
    std::vector<std::string> parts;
    std::string cur;
    cur.reserve(s.size());

    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '|') {
            parts.push_back(cur);
            cur.clear();
            continue;
        }

        if (s[i] == '\\' && i + 1 < s.size()) {
            cur.push_back(s[i]);
            cur.push_back(s[++i]);
            continue;
        }

        cur.push_back(s[i]);
    }

    parts.push_back(cur);
    return parts;
}

} // namespace

bool SchemaWorkspace::capture_from_runtime(const WorkAreaManager& wam)
{
    areas.clear();
    current_slot = wam.current_slot();

    for (int i = 0; i < wam.count(); ++i)
    {
        auto* a = wam.dbarea(i);
        if (!a) continue;

        const std::string file = a->filename();
        if (file.empty()) continue;

        SchemaAreaState s;
        s.slot       = i;
        s.path       = file;
        s.logical    = a->logicalName();
        s.recno      = a->recno();
        s.index_path = get_order_name_safe(*a);
        s.index_type = infer_index_type_from_path(s.index_path);
        s.active_tag = get_active_tag_safe(*a);

        areas.push_back(s);
    }

    return true;
}

bool SchemaWorkspace::apply_to_runtime(WorkAreaManager& wam)
{
    auto* eng = shell_engine();
    if (!eng) return false;

    for (const auto& a : areas)
    {
        try {
            eng->selectArea(a.slot);

            xbase::DbArea& area = eng->area(a.slot);

            try { orderstate::clearOrder(area); } catch (...) {}
            try { area.close(); } catch (...) {}

            area.open(a.path);
            area.setFilename(a.path);

            if (!a.logical.empty()) {
                setLogicalNameIf(area, a.logical, 0);
                setLegacyNameIf(area, a.logical, 0);
            }

            if (!a.index_path.empty() && !ci_equal(a.index_path, "none")) {
                try { orderstate::setOrder(area, a.index_path); } catch (...) {}
            }

            if (!a.active_tag.empty() && !ci_equal(a.active_tag, "none")) {
                (void)set_active_tag_safe(area, a.active_tag);
            }

            if (a.recno > 0) {
                (void)area.gotoRec(a.recno);
            }
        }
        catch (...) {
            return false;
        }
    }

    return wam.select(current_slot);
}

bool SchemaWorkspace::save_file(const std::string& path) const
{
    std::ofstream f(path);
    if (!f) return false;

    f << "DTSHEMA 3\n";
    f << "CURRENT|" << current_slot << "\n";

    for (const auto& a : areas)
    {
        f << "AREA|"
          << a.slot << "|"
          << encode_field(a.path) << "|"
          << encode_field(a.logical) << "|"
          << a.recno << "|"
          << encode_field(a.index_path) << "|"
          << encode_field(a.index_type) << "|"
          << encode_field(a.active_tag)
          << "\n";
    }

    return true;
}

bool SchemaWorkspace::load_file(const std::string& path)
{
    std::ifstream f(path);
    if (!f) return false;

    areas.clear();
    current_slot = 0;

    std::string line;
    bool saw_header = false;

    while (std::getline(f, line))
    {
        line = trim_copy(line);
        if (line.empty()) continue;

        if (!saw_header) {
            saw_header = true;

            if (ci_equal(line, "DTSHEMA 3")) {
                continue;
            }

            if (line.rfind("CURRENT ", 0) == 0) {
                current_slot = std::stoi(trim_copy(line.substr(8)));
                continue;
            }

            if (line.rfind("CURRENT|", 0) == 0) {
                current_slot = std::stoi(trim_copy(line.substr(8)));
                continue;
            }

            return false;
        }

        if (line.rfind("CURRENT|", 0) == 0)
        {
            current_slot = std::stoi(trim_copy(line.substr(8)));
            continue;
        }

        if (line.rfind("CURRENT ", 0) == 0)
        {
            current_slot = std::stoi(trim_copy(line.substr(8)));
            continue;
        }

        if (line.rfind("AREA|", 0) == 0)
        {
            const std::string body = line.substr(5);
            const auto parts = split_pipe_escaped(body);

            if (parts.size() >= 7) {
                SchemaAreaState s;
                s.slot       = std::stoi(parts[0]);
                s.path       = decode_field(parts[1]);
                s.logical    = decode_field(parts[2]);
                s.recno      = std::stoi(parts[3]);
                s.index_path = decode_field(parts[4]);
                s.index_type = decode_field(parts[5]);
                s.active_tag = decode_field(parts[6]);
                areas.push_back(s);
                continue;
            }

            return false;
        }

        if (line.rfind("AREA ", 0) == 0)
        {
            const std::string body = line.substr(5);
            const auto parts = split_pipe_escaped(body);

            if (parts.size() >= 4) {
                SchemaAreaState s;
                s.slot       = std::stoi(parts[0]);
                s.path       = decode_field(parts[1]);
                s.logical    = decode_field(parts[2]);
                s.recno      = std::stoi(parts[3]);
                s.index_path.clear();
                s.index_type = "NONE";
                s.active_tag.clear();
                areas.push_back(s);
                continue;
            }

            return false;
        }
    }

    return saw_header;
}

} // namespace dottalk::workspace
// src/cli/cmd_table.cpp
#include "cli/cmd_table.hpp"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <set>

#include "cli/table_state.hpp"
#include "xbase.hpp"

extern "C" xbase::XBaseEngine* shell_engine();

using namespace dottalk::table;

namespace {

// ---- Helpers ---------------------------------------------------------------

static inline std::string trim_copy(std::string s) {
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                    [&](unsigned char c) { return !is_space(c); }));
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [&](unsigned char c) { return !is_space(c); }).base(),
            s.end());
    return s;
}

static inline std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static inline bool try_parse_int(const std::string& s, int& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0') return false;
    if (v < INT_MIN || v > INT_MAX) return false;
    out = (int)v;
    return true;
}

static std::vector<std::string> split_tokens(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ',' || std::isspace((unsigned char)c)) {
            if (!cur.empty()) { out.push_back(trim_copy(cur)); cur.clear(); }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(trim_copy(cur));
    out.erase(std::remove_if(out.begin(), out.end(),
                             [](const std::string& t){ return t.empty(); }),
              out.end());
    return out;
}

static inline std::string basename(const std::string& path) {
    size_t p1 = path.find_last_of('\\');
    size_t p2 = path.find_last_of('/');
    size_t p = std::string::npos;
    if (p1 != std::string::npos) p = p1;
    if (p2 != std::string::npos) p = (p == std::string::npos) ? p2 : std::max(p, p2);
    if (p == std::string::npos) return path;
    return path.substr(p + 1);
}

static std::vector<int> parse_area_targets(const std::string& rest) {
    std::vector<int> out;
    std::string r = trim_copy(rest);
    if (r.empty()) return out;

    if (to_lower(r) == "all") {
        out.reserve(xbase::MAX_AREA);
        for (int i = 0; i < xbase::MAX_AREA; ++i) out.push_back(i);
        return out;
    }

    for (const auto& tok : split_tokens(r)) {
        int n = -1;
        if (try_parse_int(tok, n)) out.push_back(n);
    }
    return out;
}

// Helper: count unique recnos in tb
static size_t unique_recnos_in_tb(const TableBuffer& tb) {
    std::set<int> recnos;
    for (const auto& pair : tb.changes) {
        recnos.insert(pair.first);
    }
    return recnos.size();
}

// Optional stale fields string
static std::string stale_fields_string_for_area(xbase::DbArea& A, int area0) {
    std::vector<int> fields1;
    if (!get_stale_fields(area0, fields1)) return {};
    if (fields1.empty()) return {};

    std::string out;
    try {
        const auto defs = A.fields();
        bool first = true;
        for (int f1 : fields1) {
            const std::size_t idx0 = static_cast<std::size_t>(f1 - 1);
            if (idx0 >= defs.size()) continue;

            if (!first) out += ",";
            out += defs[idx0].name;
            first = false;

            if (out.size() > 120) { out += ",..."; break; }
        }
    } catch (...) {
        out = "(fields?)";
    }

    if (out.empty()) return {};
    return std::string(" [") + out + "]";
}

// Area index lookup
static int resolve_current_index(xbase::DbArea& A) {
    xbase::XBaseEngine* eng = shell_engine();
    if (!eng) return -1;

    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        if (&eng->area(i) == &A) return i;
    }
    return -1;
}

// ---- Display Functions -----------------------------------------------------

static void table_show(bool show_all_slots) {
    auto* eng = shell_engine();
    if (!eng) { std::cout << "TABLE BUFFER: engine not available.\n"; return; }

    const int enabled = count_enabled();
    const int dirty   = count_dirty();
    const int stale   = count_stale();

    if (show_all_slots) {
        std::cout << "TABLE BUFFER: areas 0.." << (xbase::MAX_AREA - 1) << "\n";
    } else {
        std::cout << "TABLE BUFFER: occupied areas only\n";
    }
    std::cout << "  enabled=" << enabled << " dirty=" << dirty << " stale=" << stale << "\n";

    int shown = 0;

    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        xbase::DbArea& A = eng->area(i);
        const std::string dbf = A.filename();
        const bool open = !dbf.empty();

        if (!show_all_slots && !open) continue;

        const bool en = is_enabled(i);
        const bool di = is_dirty(i);
        const bool st = is_stale(i);

        std::string staleFields;
        if (open && st) staleFields = stale_fields_string_for_area(A, i);

        std::cout << "  Area " << i << ": "
                  << (open ? ("DBF=" + basename(dbf)) : std::string("(no file)"))
                  << " | table=" << (en ? "ON" : "OFF")
                  << " | " << (di ? "DIRTY" : "clean")
                  << " | " << (st ? "STALE" : "fresh")
                  << staleFields;

        const auto& tb = get_tb_const(i);
        if (!tb.empty()) {
            std::cout << " | buffer: " << tb.changes.size()
                      << " changes (" << unique_recnos_in_tb(tb) << " recnos)";
        }

        std::cout << "\n";
        ++shown;
    }

    if (!show_all_slots && shown == 0) {
        std::cout << "  (no open tables)\n";
    }
}

static int apply_one(int area0, const std::string& verb, bool value) {
    if (!in_range(area0)) return 0;

    int changed = 0;

    if (verb == "enabled") {
        bool old = is_enabled(area0);
        if (old != value) {
            set_enabled(area0, value);
            changed = 1;
        }
    } else if (verb == "dirty") {
        bool old = is_dirty(area0);
        if (old != value) {
            set_dirty(area0, value);
            changed = 1;
        }
    } else if (verb == "stale") {
        bool old = is_stale(area0);
        if (old != value) {
            set_stale(area0, value);
            changed = 1;
        }
    }

    return changed;
}

static void apply_to_targets(const std::vector<int>& targets,
                             const std::string& verb, bool value) {
    int changed = 0;
    for (int a : targets) {
        changed += apply_one(a, verb, value);
    }
    std::cout << "TABLE BUFFER: " << changed << " area(s) updated.\n";
}

static void apply_to_open_areas(const std::string& verb, bool value) {
    auto* eng = shell_engine();
    if (!eng) { std::cout << "TABLE BUFFER: engine not available.\n"; return; }

    int changed = 0;
    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        xbase::DbArea& A = eng->area(i);
        if (A.filename().empty()) continue; // open only
        changed += apply_one(i, verb, value);
    }
    std::cout << "TABLE BUFFER: " << changed << " area(s) updated.\n";
}

// ---- BUFFER STATUS / DUMP / TESTADD ----------------------------------------

static void table_buffer_status(int area0) {
    if (!in_range(area0)) {
        std::cout << "Invalid area.\n";
        return;
    }
    const auto& tb = get_tb_const(area0);
    std::cout << "Area " << area0 << " buffer:\n";
    std::cout << "  empty: " << (tb.empty() ? "yes" : "no") << "\n";
    std::cout << "  changes: " << tb.changes.size() << "\n";
    std::cout << "  unique recnos: " << unique_recnos_in_tb(tb) << "\n";
    std::cout << "  next_seq: " << tb.next_priority << "\n";
}

static void table_buffer_dump(int area0) {
    if (!in_range(area0)) {
        std::cout << "Invalid area.\n";
        return;
    }
    const auto& tb = get_tb_const(area0);
    if (tb.empty()) {
        std::cout << "Area " << area0 << " buffer: empty\n";
        return;
    }

    std::cout << "Area " << area0 << " buffer dump:\n";
    for (const auto& pair : tb.changes) {
        const auto& entry = pair.second;
        std::cout << "  recno=" << entry.recno
                  << " seq=" << entry.priority
                  << " flags=0x" << std::hex << entry.dirty_flags << std::dec;
        bool any_field = false;
        for (int w = 0; w < kWords; ++w) {
            if (entry.field_bits[w] != 0) { any_field = true; break; }
        }
        if (any_field) std::cout << " (fields changed)";
        if (!entry.new_values.empty()) {
            std::cout << " values:";
            for (const auto& [f, v] : entry.new_values) {
                std::cout << " #" << f << "=\"" << v << "\"";
            }
        }
        std::cout << "\n";
    }
}

static void table_buffer_command(const std::string& rest, xbase::DbArea& current_area) {
    std::string r = trim_copy(rest);
    if (r.empty()) {
        std::cout << "Usage: TABLE BUFFER: STATUS [area|ALL]\n"
                  << "       TABLE BUFFER: DUMP [area|ALL]\n"
                  << "       TABLE BUFFER: TESTADD <recno> [flags] [field1] [value]\n";
        return;
    }

    auto toks = split_tokens(r);
    if (toks.empty()) return;
    std::string sub = to_lower(toks[0]);
    std::string arg = (toks.size() > 1) ? trim_copy(r.substr(toks[0].size())) : "";

    auto* eng = shell_engine();
    if (!eng) return;

    if (sub == "status") {
        if (to_lower(arg) == "all") {
            for (int i = 0; i < xbase::MAX_AREA; ++i) {
                if (!is_enabled(i)) continue;
                table_buffer_status(i);
            }
        } else if (arg.empty()) {
            int curr = resolve_current_index(current_area);
            if (curr >= 0 && is_enabled(curr)) {
                table_buffer_status(curr);
            } else {
                std::cout << "No current area selected or not enabled.\n";
            }
        } else {
            int a = -1;
            if (try_parse_int(arg, a)) table_buffer_status(a);
            else std::cout << "Invalid area.\n";
        }
    } else if (sub == "dump") {
        if (to_lower(arg) == "all") {
            for (int i = 0; i < xbase::MAX_AREA; ++i) {
                if (!is_enabled(i)) continue;
                table_buffer_dump(i);
            }
        } else if (arg.empty()) {
            int curr = resolve_current_index(current_area);
            if (curr >= 0 && is_enabled(curr)) {
                table_buffer_dump(curr);
            } else {
                std::cout << "No current area selected or not enabled.\n";
            }
        } else {
            int a = -1;
            if (try_parse_int(arg, a)) table_buffer_dump(a);
            else std::cout << "Invalid area.\n";
        }
    } else if (sub == "testadd") {
        if (arg.empty()) {
            std::cout << "Usage: TABLE BUFFER: TESTADD <recno> [flags] [field1] [value]\n";
            return;
        }

        // Remove comment from arg
        size_t comment_pos = arg.find("//");
        if (comment_pos != std::string::npos) {
            arg = arg.substr(0, comment_pos);
        }
        arg = trim_copy(arg);

        std::istringstream iss(arg);
        int recno = -1;
        iss >> recno;
        if (recno < 1) {
            std::cout << "Invalid recno.\n";
            return;
        }

        std::uint64_t flags = CHANGE_UPDATE;
        int field1 = 0;
        std::string value;

        // flags (optional number)
        std::string token;
        if (iss >> token) {
            int f = 0;
            if (try_parse_int(token, f)) {
                flags = static_cast<std::uint64_t>(f);
            } else {
                value = token;
            }
        }

        // field1 (optional number)
        if (iss >> token) {
            int f = 0;
            if (try_parse_int(token, f)) {
                field1 = f;
            } else {
                value = token;
            }
        }

        // value: take the rest of the line
        std::getline(iss, token);
        if (!token.empty()) {
            value = trim_copy(token);
        }

        std::cout << "Parsed: recno=" << recno << ", flags=0x" << std::hex << flags << std::dec
                  << ", field1=" << field1 << ", value='" << value << "'\n";

        int curr = resolve_current_index(current_area);
        if (curr < 0 || !is_enabled(curr)) {
            std::cout << "No current enabled area.\n";
            return;
        }

// hard code these two values
//      field1 = 3;
//      value = "DeraldNew";

        test_add_change(curr, recno, flags, field1, value);
    } else {
        std::cout << "Unknown BUFFER subcommand: " << sub << "\n";
    }
}

} // namespace

void cmd_TABLE(xbase::DbArea& current, std::istringstream& in) {
    std::string arg_line;
    std::getline(in, arg_line);
    std::string args = trim_copy(arg_line);

    // Compatibility: "TABLES" alias
    if (!args.empty()) {
        const auto toks = split_tokens(args);
        if (!toks.empty() && to_lower(toks[0]) == "tables") {
            const bool show_all_slots = (toks.size() > 1 && to_lower(toks[1]) == "all");
            table_show(show_all_slots);
            return;
        }
    }

    // Default: TABLE => show occupied only
    if (args.empty()) {
        table_show(false);
        return;
    }

    // Parse first token as subcommand
    std::string sub, rest;
    auto sp = args.find_first_of(" \t");
    if (sp == std::string::npos) { sub = args; rest = ""; }
    else { sub = args.substr(0, sp); rest = trim_copy(args.substr(sp)); }
    sub = to_lower(sub);

    // Explicit ALL view
    if (sub == "all") {
        table_show(true);
        return;
    }

    // SHOW variants
    if (sub == "status" || sub == "show" || sub == "list" || sub == "s") {
        const bool show_all_slots = (to_lower(trim_copy(rest)) == "all");
        table_show(show_all_slots);
        return;
    }

    // BUFFER commands
    if (sub == "buffer") {
        table_buffer_command(rest, current);
        return;
    }

    auto* eng = shell_engine();
    if (!eng) { std::cout << "TABLE BUFFER: engine not available.\n"; return; }

    auto default_current_target = [&]() -> std::vector<int> {
        for (int i = 0; i < xbase::MAX_AREA; ++i) {
            if (&eng->area(i) == &current) return { i };
        }
        return {};
    };

    // ALL variants (open areas only)
    if (sub == "onall")  { apply_to_open_areas("enabled", true);  return; }
    if (sub == "offall") { apply_to_open_areas("enabled", false); return; }
    if (sub == "dirtyall") { apply_to_open_areas("dirty", true);  return; }
    if (sub == "cleanall" || sub == "clearall") { apply_to_open_areas("dirty", false); return; }
    if (sub == "staleall") { apply_to_open_areas("stale", true);  return; }
    if (sub == "freshall" || sub == "clearstaleall" || sub == "unstaleall") {
        apply_to_open_areas("stale", false); return;
    }

    // Parse targets
    std::vector<int> targets = parse_area_targets(rest);
    if (targets.empty() && to_lower(rest) != "all") {
        targets = default_current_target();
        if (targets.empty()) {
            std::cout << "TABLE BUFFER: cannot determine current area; specify an area number.\n";
            return;
        }
    }

    if (sub == "on" || sub == "enable" || sub == "enabled") {
        apply_to_targets(targets, "enabled", true);
    } else if (sub == "off" || sub == "disable") {
        apply_to_targets(targets, "enabled", false);
    } else if (sub == "dirty") {
        apply_to_targets(targets, "dirty", true);
    } else if (sub == "clean" || sub == "clear") {
        apply_to_targets(targets, "dirty", false);
    } else if (sub == "stale") {
        apply_to_targets(targets, "stale", true);
    } else if (sub == "fresh" || sub == "clearstale" || sub == "unstale") {
        apply_to_targets(targets, "stale", false);
    } else if (sub == "reset") {
        reset_all();
        std::cout << "TABLE BUFFER: reset all areas.\n";
    } else {
        std::cout << "TABLE BUFFER: unknown subcommand '" << sub << "'.\n";
        std::cout << "Usage:\n";
        std::cout << "  TABLE\n";
        std::cout << "  TABLE ALL\n";
        std::cout << "  TABLE [STATUS|SHOW|LIST|S] [ALL]\n";
        std::cout << "  TABLE ON  [<n>|ALL|n,m,...]\n";
        std::cout << "  TABLE OFF [<n>|ALL|n,m,...]\n";
        std::cout << "  TABLE DIRTY [<n>|ALL|n,m,...]\n";
        std::cout << "  TABLE CLEAN [<n>|ALL|n,m,...]\n";
        std::cout << "  TABLE STALE [<n>|ALL|n,m,...]\n";
        std::cout << "  TABLE FRESH [<n>|ALL|n,m,...]\n";
        std::cout << "  TABLE BUFFER: STATUS [area|ALL]\n";
        std::cout << "  TABLE BUFFER: DUMP [area|ALL]\n";
        std::cout << "  TABLE BUFFER: TESTADD <recno> [flags] [field1] [value]\n";
        std::cout << "  TABLE ONALL|OFFALL              (open areas only)\n";
        std::cout << "  TABLE DIRTYALL|CLEANALL         (open areas only)\n";
        std::cout << "  TABLE STALEALL|FRESHALL         (open areas only)\n";
        std::cout << "  TABLE RESET\n";
    }
}
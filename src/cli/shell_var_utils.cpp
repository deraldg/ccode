#include "shell_var_utils.hpp"

#include "textio.hpp"
#include "shell_eval_utils.hpp"  // for VarBangEval, eval_for_varbang, serialize_varbang_value
#include <algorithm>
#include <cctype>
#include <iostream>   // for std::cout
#include <iomanip>    // for std::setprecision
#include <sstream>
#include <string>

namespace dottalk {

std::unordered_map<std::string, std::string> g_shell_vars;

bool is_ident_start(char c) {
    return (std::isalpha(static_cast<unsigned char>(c)) || c == '_');
}

bool is_ident_char(char c) {
    return (std::isalnum(static_cast<unsigned char>(c)) || c == '_');
}

bool expand_macros_outside_quotes(const std::string& in,
                                  std::string& out,
                                  std::string& err_name)
{
    out.clear();
    out.reserve(in.size() + 16);
    bool in_single = false, in_double = false, esc = false;
    for (size_t i = 0; i < in.size(); ++i) {
        const char c = in[i];
        if (esc) { esc = false; out.push_back(c); continue; }
        if (c == '\\') { esc = true; out.push_back(c); continue; }
        if (!in_double && c == '\'') { in_single = !in_single; out.push_back(c); continue; }
        if (!in_single && c == '\"') { in_double = !in_double; out.push_back(c); continue; }
        if (!in_single && !in_double && c == '&') {
            if (i + 1 < in.size() && is_ident_start(in[i + 1])) {
                size_t j = i + 2;
                while (j < in.size() && is_ident_char(in[j])) ++j;
                const std::string name = in.substr(i + 1, j - (i + 1));
                const std::string key = textio::up(name);
                auto it = g_shell_vars.find(key);
                if (it == g_shell_vars.end()) {
                    err_name = name;
                    return false;
                }
                out.append(it->second);
                i = j - 1;
                continue;
            }
        }
        out.push_back(c);
    }
    return true;
}

std::string quote_dottalk_string(const std::string& raw) {
    std::string out;
    out.reserve(raw.size() + 2);
    out.push_back('"');
    for (char c : raw) {
        if (c == '"') out.append("\"\"");
        else out.push_back(c);
    }
    out.push_back('"');
    return out;
}

VarCmdResult try_handle_var_command(xbase::DbArea& area, const std::string& preparedLine)
{
    VarCmdResult r;
    std::istringstream ss(preparedLine);
    std::string cmd;
    ss >> cmd;
    if (cmd.empty()) return r;
    const std::string U = textio::up(cmd);
    std::string sub;
    ss >> sub;
    const std::string subU = textio::up(sub);
    const bool var_bang = (subU == "VAR!");
    if (!(subU == "VAR" || var_bang)) return r;
    r.handled = true;
    std::string rest;
    std::getline(ss, rest);
    rest = textio::trim(rest);
    if (U == "SET") {
        const size_t eq = rest.find('=');
        if (eq == std::string::npos) {
            std::cout << "Usage: SET VAR <name> = <text>\n";
            r.ok = false;
            return r;
        }
        std::string name = textio::trim(rest.substr(0, eq));
        std::string val = textio::trim(rest.substr(eq + 1));
        if (name.empty()) {
            std::cout << "Usage: SET VAR <name> = <text>\n";
            r.ok = false;
            return r;
        }
        if (!is_ident_start(name[0])) {
            std::cout << "SET VAR: invalid name: " << name << "\n";
            r.ok = false;
            return r;
        }
        for (char c : name) {
            if (!is_ident_char(c)) {
                std::cout << "SET VAR: invalid name: " << name << "\n";
                r.ok = false;
                return r;
            }
        }
        if (var_bang) {
            VarBangEval ev;
            std::string err;
            if (!eval_for_varbang(area, val, ev, err)) {
                std::cout << "SET VAR!: " << err << "\n";
                r.ok = false;
                return r;
            }
            g_shell_vars[textio::up(name)] = serialize_varbang_value(ev);
            return r;
        }
        g_shell_vars[textio::up(name)] = val;
        return r;
    }
    if (U == "SHOW") {
        if (rest.empty()) {
            std::vector<std::string> keys;
            keys.reserve(g_shell_vars.size());
            for (const auto& kv : g_shell_vars) keys.push_back(kv.first);
            std::sort(keys.begin(), keys.end());
            std::cout << "VARS: " << keys.size() << " defined.\n";
            for (const auto& k : keys) {
                std::cout << " " << k << " = " << g_shell_vars[k] << "\n";
            }
            return r;
        }
        std::istringstream rs(rest);
        std::string name, extra;
        rs >> name;
        rs >> extra;
        if (name.empty() || !extra.empty()) {
            std::cout << "Usage: SHOW VAR [<name>]\n";
            r.ok = false;
            return r;
        }
        const std::string key = textio::up(name);
        auto it = g_shell_vars.find(key);
        if (it == g_shell_vars.end()) {
            std::cout << "VAR not defined: " << name << "\n";
            r.ok = false;
            return r;
        }
        std::cout << key << " = " << it->second << "\n";
        return r;
    }
    if (U == "CLEAR") {
        if (rest.empty()) {
            std::cout << "Usage: CLEAR VAR <name|ALL>\n";
            r.ok = false;
            return r;
        }
        std::istringstream rs(rest);
        std::string name, extra;
        rs >> name;
        rs >> extra;
        if (name.empty() || !extra.empty()) {
            std::cout << "Usage: CLEAR VAR <name|ALL>\n";
            r.ok = false;
            return r;
        }
        const std::string key = textio::up(name);
        if (key == "ALL") {
            g_shell_vars.clear();
            return r;
        }
        auto it = g_shell_vars.find(key);
        if (it == g_shell_vars.end()) {
            std::cout << "VAR not defined: " << name << "\n";
            r.ok = false;
            return r;
        }
        g_shell_vars.erase(it);
        return r;
    }
    std::cout << "Usage: SET VAR | SHOW VAR | CLEAR VAR\n";
    r.ok = false;
    return r;
}

} // namespace dottalk
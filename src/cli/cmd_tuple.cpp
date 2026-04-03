// src/cli/cmd_tuple.cpp
// cmd_TUPLE delegates tuple assembly to the canonical builder (tuple_builder.*).
// Printing and CLI flags remain here; data truth lives in the builder.

#include <algorithm>
#include <cctype>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "textio.hpp"
#include "tuple_builder.hpp"
#include "tuple_types.hpp"
#include "xbase.hpp"

namespace {
constexpr char US = '\x1F';

std::string trim(std::string s) {
    auto issp = [](unsigned char c) { return std::isspace(c) != 0; };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&](unsigned char c) { return !issp(c); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [&](unsigned char c) { return !issp(c); }).base(), s.end());
    return s;
}

std::string up(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

// Extracts and removes known flags from the spec (case-insensitive match against uppercase flag).
bool remove_flag_ci(std::string& s, const std::string& flag_upper) {
    std::string u = up(s);
    bool removed = false;

    for (;;) {
        const auto pos = u.find(flag_upper);
        if (pos == std::string::npos) break;

        const bool left_ok = (pos == 0) || std::isspace(static_cast<unsigned char>(u[pos - 1])) || u[pos - 1] == ',';
        const std::size_t end = pos + flag_upper.size();
        const bool right_ok = (end >= u.size()) || std::isspace(static_cast<unsigned char>(u[end])) || u[end] == ',';

        if (!left_ok || !right_ok) {
            u[pos] = '\x01';
            continue;
        }

        s.erase(pos, flag_upper.size());
        u.erase(pos, flag_upper.size());
        removed = true;
    }

    s = trim(s);

    // normalize whitespace
    std::string out;
    out.reserve(s.size());
    bool prev_space = false;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!prev_space) out.push_back(' ');
            prev_space = true;
        } else {
            out.push_back(c);
            prev_space = false;
        }
    }
    s.swap(out);
    return removed;
}

std::string join_us(const std::vector<std::string>& vals) {
    std::string out;
    for (std::size_t i = 0; i < vals.size(); ++i) {
        if (i) out.push_back(US);
        out += vals[i];
    }
    return out;
}

std::string join_pretty(const std::vector<std::string>& vals, const std::string& null_token) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < vals.size(); ++i) {
        if (i) oss << " | ";
        const std::string& v = vals[i];
        if (v.empty()) {
            if (!null_token.empty()) oss << null_token;
            else oss << "\"\"";
        } else {
            oss << v;
        }
    }
    return oss.str();
}

} // namespace

void cmd_TUPLE(xbase::DbArea& /*A*/, std::istringstream& args) {
    std::string spec{std::istreambuf_iterator<char>(args), std::istreambuf_iterator<char>()};
    spec = trim(spec);
    if (spec.empty()) spec = "*";

    bool want_header = false;
    bool header_area_prefix = false;
    bool want_echo = true;
    bool strict_fields = false;
    bool header_only = false;
    bool values_only = false;
    bool debug_raw = false;
    std::string null_token;

    {
        std::string s2 = spec;

        if (remove_flag_ci(s2, "--HEADER")) want_header = true;
        if (remove_flag_ci(s2, "--AREA-PREFIX")) header_area_prefix = true;
        if (remove_flag_ci(s2, "--NO-ECHO")) want_echo = false;
        if (remove_flag_ci(s2, "--STRICT")) strict_fields = true;
        if (remove_flag_ci(s2, "--HEADER-ONLY")) header_only = true;
        if (remove_flag_ci(s2, "--VALUES-ONLY")) values_only = true;

        // "DEBUG" or "--DEBUG" => print raw US-separated line as well.
        if (remove_flag_ci(s2, "DEBUG") || remove_flag_ci(s2, "--DEBUG")) debug_raw = true;

        // Very small parser for: --null TOKEN
        {
            std::string u = up(s2);
            const auto p = u.find("--NULL");
            if (p != std::string::npos) {
                s2.erase(p, 6);
                s2 = trim(s2);
                std::istringstream iss(s2);
                std::string tok;
                if (iss >> tok) null_token = tok;
            }
        }

        spec = trim(s2);
        if (spec.empty()) spec = "*";
    }

    dottalk::TupleBuildOptions opt;
    opt.want_header = false; // header printed here
    opt.header_area_prefix = header_area_prefix;
    opt.strict_fields = strict_fields;
    opt.null_token = null_token;

    // Header row (names)
    if (want_header && !values_only) {
        auto r = dottalk::build_tuple_from_spec(spec, opt);
        if (!r.ok) {
            std::cout << r.error << "\n";
            return;
        }

        std::vector<std::string> cols;
        cols.reserve(r.row.columns.size());
        for (const auto& c : r.row.columns) cols.push_back(c.name);

        const std::string raw_h = join_us(cols);
        const std::string pretty_h = join_pretty(cols, std::string{});

        if (debug_raw || values_only || !want_echo) {
            std::cout << raw_h << "\n";
        }
        if (want_echo && !values_only) {
            std::cout << pretty_h << "\n";
        }

        if (header_only) return;
    }

    if (header_only) return;

    // Value row
    auto r = dottalk::build_tuple_from_spec(spec, opt);
    if (!r.ok) {
        std::cout << r.error << "\n";
        return;
    }

    if (r.row.values.empty()) {
        std::cout << "(empty)\n";
        return;
    }

    const std::string raw = join_us(r.row.values);
    const std::string pretty = join_pretty(r.row.values, null_token);

    // New rule:
    //   default => formatted only
    //   DEBUG/--DEBUG => formatted + raw
    //   --VALUES-ONLY => raw only
    // Compatibility:
    //   --NO-ECHO => raw only (legacy scripting behavior)
    if (values_only || (!want_echo && !debug_raw)) {
        std::cout << raw << "\n";
        return;
    }

    if (debug_raw) std::cout << raw << "\n";
    std::cout << pretty << "\n";
}

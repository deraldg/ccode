// src/cli/cmd_set_relation.cpp
//
// FoxPro-compatible SET RELATION parser
//
// Supported syntax:
//   SET RELATION TO <expr> INTO <child>
//   SET RELATION TO <expr> INTO <child>, <expr> INTO <child> ...
//   SET RELATION ADDITIVE TO <expr> INTO <child>
//   SET RELATION OFF ALL
//   SET RELATION OFF INTO <child>
//
// Notes:
// - Uses the existing relations_api backend from set_relations.cpp.
// - Does not synthesize REL text and re-parse it.
// - Current implementation treats <expr> as a field list string and reuses
//   split_fields_csv semantics via comma splitting if needed later.

#include "xbase.hpp"
#include "set_relations.hpp"
#include "textio.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

static std::string up_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

static std::string trim_copy(std::string s) {
    return textio::trim(std::move(s));
}

struct Clause {
    std::string expr_text;
    std::string child;
};

static bool parse_clause_list(const std::string& input, std::vector<Clause>& out) {
    std::size_t pos = 0;
    const std::size_t n = input.size();

    auto skip_ws = [&]() {
        while (pos < n && std::isspace(static_cast<unsigned char>(input[pos]))) ++pos;
    };

    while (true) {
        skip_ws();
        if (pos >= n) break;

        // Find INTO at top level.
        std::size_t into_pos = std::string::npos;
        for (std::size_t i = pos; i < n; ++i) {
            if (std::toupper(static_cast<unsigned char>(input[i])) == 'I') {
                std::size_t j = i;
                const char* kw = "INTO";
                bool ok = true;
                for (int k = 0; k < 4; ++k, ++j) {
                    if (j >= n || std::toupper(static_cast<unsigned char>(input[j])) != kw[k]) {
                        ok = false;
                        break;
                    }
                }
                if (ok) {
                    into_pos = i;
                    break;
                }
            }
        }

        if (into_pos == std::string::npos) return false;

        std::string expr = trim_copy(input.substr(pos, into_pos - pos));
        if (expr.empty()) return false;

        pos = into_pos + 4; // skip INTO
        skip_ws();

        std::size_t child_start = pos;
        while (pos < n &&
               !std::isspace(static_cast<unsigned char>(input[pos])) &&
               input[pos] != ',') {
            ++pos;
        }

        std::string child = trim_copy(input.substr(child_start, pos - child_start));
        if (child.empty()) return false;

        out.push_back(Clause{expr, child});

        skip_ws();
        if (pos < n && input[pos] == ',') {
            ++pos;
            continue;
        }
        break;
    }

    return !out.empty();
}

static std::vector<std::string> expr_to_fields(const std::string& expr_text) {
    // For now, preserve current backend semantics:
    // field names separated by commas.
    // Future expression support can extend this later.
    std::vector<std::string> fields;
    std::string cur;
    std::istringstream ss(expr_text);
    while (std::getline(ss, cur, ',')) {
        cur = trim_copy(cur);
        if (!cur.empty()) fields.push_back(cur);
    }
    return fields;
}

static std::string current_parent_or_empty() {
    return relations_api::current_parent_name();
}

} // namespace

void cmd_SET_RELATION(xbase::DbArea& /*A*/, std::istringstream& iss) {
    std::string rest;
    std::getline(iss, rest);
    rest = trim_copy(rest);

    if (rest.empty()) {
        std::cout
            << "SET RELATION syntax\n"
            << "  SET RELATION TO <expr> INTO <child>\n"
            << "  SET RELATION TO <expr1> INTO <child1>[, <expr2> INTO <child2> ...]\n"
            << "  SET RELATION ADDITIVE TO <expr> INTO <child>\n"
            << "  SET RELATION OFF INTO <child>\n"
            << "  SET RELATION OFF ALL\n";
        return;
    }

    std::istringstream ss(rest);
    std::string first;
    ss >> first;
    const std::string op1 = up_copy(first);

    const std::string parent = current_parent_or_empty();
    if (parent.empty()) {
        std::cout << "SET RELATION: no current parent area\n";
        return;
    }

    // ------------------------------------------------------------
    // OFF ...
    // ------------------------------------------------------------
    if (op1 == "OFF") {
        std::string second;
        ss >> second;
        const std::string op2 = up_copy(second);

        if (op2 == "ALL") {
            relations_api::clear_relations(parent);
            std::cout << "OK\n";
            return;
        }

        if (op2 == "INTO") {
            std::string child;
            ss >> child;
            child = trim_copy(child);
            if (child.empty()) {
                std::cout << "SET RELATION OFF INTO requires child area\n";
                return;
            }

            if (!relations_api::remove_relation(parent, child)) {
                return;
            }
            relations_api::refresh_if_enabled();
            std::cout << "OK\n";
            return;
        }

        std::cout << "SET RELATION: expected INTO <child> or ALL after OFF\n";
        return;
    }

    // ------------------------------------------------------------
    // [ADDITIVE] TO ...
    // ------------------------------------------------------------
    bool additive = false;
    std::string remaining;

    if (op1 == "ADDITIVE") {
        additive = true;
        std::getline(ss, remaining);
        remaining = trim_copy(remaining);

        if (remaining.empty()) {
            std::cout << "SET RELATION ADDITIVE requires TO <expr> INTO <child>\n";
            return;
        }

        std::istringstream ts(remaining);
        std::string to_kw;
        ts >> to_kw;
        if (up_copy(to_kw) != "TO") {
            std::cout << "SET RELATION ADDITIVE expects TO\n";
            return;
        }

        std::getline(ts, remaining);
        remaining = trim_copy(remaining);
    } else if (op1 == "TO") {
        std::getline(ss, remaining);
        remaining = trim_copy(remaining);
    } else {
        std::cout << "SET RELATION: expected TO, ADDITIVE, or OFF\n";
        return;
    }

    std::vector<Clause> clauses;
    if (!parse_clause_list(remaining, clauses)) {
        std::cout << "SET RELATION: invalid TO ... INTO ... clause\n";
        return;
    }

    if (!additive) {
        relations_api::clear_relations(parent);
    }

    for (const auto& c : clauses) {
        auto fields = expr_to_fields(c.expr_text);
        if (fields.empty()) {
            std::cout << "SET RELATION: empty expression for child " << c.child << "\n";
            return;
        }

        if (!relations_api::add_relation(parent, c.child, fields)) {
            return;
        }
    }

    relations_api::refresh_if_enabled();
    std::cout << "OK\n";
}
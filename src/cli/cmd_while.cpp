// ============================================================================
// path: src/cli/cmd_while.cpp
// purpose: WHILE / ENDWHILE with private buffering (no loop_state deps)
// usage:
//   WHILE <bool-expr> [QUIET]  -> begin buffering; shell must route lines to WHILE_BUFFER
//   ENDWHILE                   -> execute buffered body while <expr> remains true
// notes:
//   - Boolean evaluator is provided by shell via while_set_condition_eval(...).
//   - Starts from the CURRENT record (does NOT force TOP).
//   - Advances with A.skip(1) and stops hard at EOF/BOF or empty tables.
//   - Buffer persists after ENDWHILE (mirrors ENDLOOP behavior).
// ============================================================================

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>

#include "xbase.hpp"
#include "cmd_loop.hpp"  // for loop_get_executor() signature only

using bool_eval_t = bool(*)(xbase::DbArea&, const std::string&);
static bool_eval_t g_eval = nullptr;

extern "C" void while_set_condition_eval(bool_eval_t fn) { g_eval = fn; }

// ----------------------------- tiny state -----------------------------------
namespace whilestate {
struct State {
    bool active{false};
    bool quiet{false};
    std::string cond;
    std::vector<std::string> body;
};
static State& st() { static State S; return S; }
}

// Expose capture flag so shell can route lines into WHILE_BUFFER
extern "C" bool while_is_active() { return whilestate::st().active; }

// ------------------------------ helpers -------------------------------------
static std::string slurp_rest(std::istringstream& S) {
    std::string rest;
    std::getline(S, rest);
    if (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t')) rest.erase(0, 1);
    return rest;
}

static bool advance_one_record(xbase::DbArea& A) {
    try {
        if (!A.isOpen() || A.recCount() == 0) return false;
        return A.skip(1);
    } catch (...) {
        return false;
    }
}

static void trim_trailing(std::string& s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.pop_back();
    }
}

// -------------------------------- WHILE -------------------------------------
void cmd_WHILE(xbase::DbArea&, std::istringstream& S)
{
    auto& st = whilestate::st();

    std::string expr = slurp_rest(S);
    bool quiet = false;

    // optional trailing QUIET
    {
        std::string up = expr;
        for (auto& c : up) c = char(std::toupper((unsigned char)c));
        const auto pos = up.rfind(" QUIET");
        if (pos != std::string::npos && pos + 6 == up.size()) {
            quiet = true;
            expr.erase(pos);
            trim_trailing(expr);
        }
    }

    st.body.clear();
    st.active = true;
    st.quiet  = quiet;
    st.cond   = std::move(expr);

    if (!st.quiet) {
        std::cout << "WHILE: buffering body. Type ENDWHILE to execute loop.\n";
    }
}

// Lines are routed here by the shell while capture is active
void cmd_WHILE_BUFFER(xbase::DbArea&, std::istringstream& S)
{
    auto& st = whilestate::st();
    std::string raw;
    std::getline(S, raw);
    if (!raw.empty() && (raw[0] == ' ' || raw[0] == '\t')) raw.erase(0, 1);
    if (!raw.empty()) st.body.push_back(std::move(raw));
}

// -------------------------------- ENDWHILE ----------------------------------
void cmd_ENDWHILE(xbase::DbArea& A, std::istringstream&)
{
    auto& st = whilestate::st();

    // stop capturing before we run
    st.active = false;

    if (!g_eval) {
        if (!st.quiet) {
            std::cout << "ENDWHILE: no condition evaluator; nothing executed.\n";
        }
        return;
    }

    // Start from CURRENT record. Do not force TOP.

    // copy snapshot to avoid surprise mutations during execution
    const auto body = st.body;
    std::size_t iters = 0;

    while (g_eval(A, st.cond)) {
        ++iters;

        if (!body.empty()) {
            auto* exec = loop_get_executor();
            if (exec) {
                for (const auto& line : body) {
                    (*exec)(A, line);
                }
            }
        }

        if (!advance_one_record(A)) break;
    }

    if (!st.quiet) {
        std::cout << "ENDWHILE: executed " << iters
                  << " iteration(s) over " << body.size() << " line(s).\n";
    }
}
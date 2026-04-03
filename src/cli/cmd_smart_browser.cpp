// src/cli/cmd_smart_browser.cpp
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "db_tuple_stream.hpp"
#include "schema_loader.hpp"
#include "relations_status.hpp"
#include "set_relations.hpp"
#include "workareas.hpp"
#include "expr_tuple_glue.hpp"

namespace {

// Preserve/restore *all* open workarea cursors.
//
// Smart Browser is intended to be read-only. It drives a DbTupleStream that
// may move record pointers in various DbAreas as it reads/joins. We snapshot
// each open workarea's recno on entry and restore them on exit, then run a
// relations refresh (if enabled) to re-sync children to the restored parent.
struct WorkAreaCursorRestore {
    struct SlotState {
        int32_t recno{0};
        bool open{false};
    };

    std::vector<SlotState> slots;

    WorkAreaCursorRestore() {
        const std::size_t n = workareas::count();
        slots.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            if (xbase::DbArea* a = workareas::at(i)) {
                try {
                    slots[i].open  = a->isOpen();
                    slots[i].recno = a->recno();
                } catch (...) {
                    slots[i].open = false;
                    slots[i].recno = 0;
                }
            }
        }
    }

    ~WorkAreaCursorRestore() {
        for (std::size_t i = 0; i < slots.size(); ++i) {
            if (!slots[i].open) continue;
            if (slots[i].recno <= 0) continue;
            if (xbase::DbArea* a = workareas::at(i)) {
                try {
                    if (a->isOpen()) {
                        (void)a->gotoRec(slots[i].recno);
                        (void)a->readCurrent();
                    }
                } catch (...) {}
            }
        }

        // Re-sync child areas to the restored parent cursor.
        try { relations_api::refresh_if_enabled(); } catch (...) {}
    }
};

static std::string trim(std::string s) {
    auto issp = [](unsigned char c){ return c==' '||c=='\t'||c=='\r'||c=='\n'; };
    while(!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
    while(!s.empty() && issp((unsigned char)s.back())) s.pop_back();
    return s;
}
static std::string up(std::string s) { for (auto& c : s) c = (char)std::toupper((unsigned char)c); return s; }

struct PagerState { bool show_schema=false, show_json=false, status_verbose=false; };
struct StreamCtx { std::string spec; std::string filter; };

static void print_tuple_row(const dottalk::TupleRow& r) {
    if (!r.fragments.empty() && r.fragments.front().recno > 0) {
        std::cout << "RECNO=" << r.fragments.front().recno << " | ";
    }
    for (size_t i = 0; i < r.columns.size() && i < r.values.size(); ++i) {
        if (i) std::cout << " | ";
        std::cout << r.columns[i].name << "=" << (r.values[i].empty() ? "\"\"" : r.values[i]);
    }
    std::cout << "\n";
}

static void show_breadcrumbs(const std::vector<StreamCtx>& stack, const StreamCtx& cur) {
    std::cout << "BREADCRUMBS:\n";
    if (stack.empty()) { std::cout << "  (root) " << (cur.spec.empty() ? "*" : cur.spec) << "\n"; return; }
    for (size_t i=0;i<stack.size();++i) std::cout << "  " << i+1 << ") " << (stack[i].spec.empty()?"*":stack[i].spec) << "\n";
    std::cout << "  > " << (cur.spec.empty()?"*":cur.spec) << "\n";
}

enum class PagerAction { NextPage, Quit, Repaint };
struct PromptResult { PagerAction action; bool nav_event; };

static PromptResult dispatch(dottalk::DbTupleStream& stream, PagerState& ps,
                             std::vector<StreamCtx>& crumbs, StreamCtx& cur) {
    std::cout
        << "-- More -- (Enter=next, Q=quit, TOP/BOTTOM/SKIP n/GOTO n, FOR <expr>, CLEAR FOR, ORDER PHYSICAL/INX/CNX, "
        << "SPEC <spec>, SHOW|HIDE SCHEMA|JSON, SHOW CHILDREN [LIMIT n], SHOW CHILDREN FOR <area> PREVIEW n, "
        << "OPEN CHILD <area>, BACK, SHOW BREADCRUMBS, STATUS VERBOSE|COMPACT, HELP) ";
    std::string line; if (!std::getline(std::cin, line)) return {PagerAction::Quit, false};
    const std::string raw = line;
    auto trim_copy = [](std::string s){ auto sp=[](unsigned char c){return c==' '||c=='\t'||c=='\r'||c=='\n';};
        while(!s.empty()&&sp((unsigned char)s.front())) s.erase(s.begin());
        while(!s.empty()&&sp((unsigned char)s.back())) s.pop_back(); return s; };
    std::string u = up(trim_copy(line));
    if (u.empty()) return {PagerAction::NextPage, false};
    if (u == "Q" || u == "QUIT") return {PagerAction::Quit, false};

    if (u == "HELP") {
        std::cout
          << "Smart Browser Help\n"
          << "  OPEN CHILD <area> / BACK / SHOW BREADCRUMBS\n"
          << "  SHOW CHILDREN [LIMIT n]\n"
          << "  SHOW CHILDREN FOR <area> PREVIEW n\n";
        return {PagerAction::Repaint, false};
    }

    if (u == "SHOW BREADCRUMBS") { show_breadcrumbs(crumbs, cur); return {PagerAction::Repaint, false}; }

    if (u.rfind("OPEN CHILD ", 0) == 0) {
        std::string area = trim_copy(raw.substr(11));
        if (area.empty()) { std::cout << "Usage: OPEN CHILD <area>\n"; return {PagerAction::Repaint, false}; }
        crumbs.push_back(cur);
        cur.spec = area + ".*";
        cur.filter.clear();
        stream.set_spec(cur.spec);
        stream.set_filter_for(cur.filter);
        stream.top();
        return {PagerAction::Repaint, true};
    }

    if (u == "BACK") {
        if (crumbs.empty()) { std::cout << "BACK: at root.\n"; return {PagerAction::Repaint, false}; }
        StreamCtx prev = crumbs.back(); crumbs.pop_back();
        cur = prev;
        stream.set_spec(cur.spec);
        stream.set_filter_for(cur.filter);
        stream.top();
        return {PagerAction::Repaint, true};
    }

    if (u.rfind("SPEC ", 0) == 0) {
        cur.spec = trim_copy(raw.substr(5));
        stream.set_spec(cur.spec);
        return {PagerAction::Repaint, true};
    }

    if (u == "CLEAR FOR" || u == "CLEARFOR") {
        cur.filter.clear();
        stream.set_filter_for("");
        stream.top();
        return {PagerAction::Repaint, true};
    }

    if (u.rfind("FOR ", 0) == 0) {
        cur.filter = trim_copy(raw.substr(4));
        stream.set_filter_for(cur.filter);
        stream.top();
        return {PagerAction::Repaint, true};
    }

    if (u.rfind("SHOW CHILDREN FOR ", 0) == 0) {
        std::string rest = trim_copy(raw.substr(18));
        auto pos_prev = up(rest).find("PREVIEW");
        if (pos_prev == std::string::npos) { std::cout << "Usage: SHOW CHILDREN FOR <area> PREVIEW n\n"; return {PagerAction::Repaint, false}; }
        std::string area = trim_copy(rest.substr(0, pos_prev));
        int limit = 5; try { std::string ns = trim_copy(rest.substr(pos_prev + 7)); limit = std::stoi(ns); } catch (...) {}
        if (limit < 1) limit = 1; if (limit > 100) limit = 100;
        try {
            auto rows = relations_api::preview_child(area, limit);
            if (rows.empty()) std::cout << "CHILDREN PREVIEW (" << area << "): (none)\n";
            else { std::cout << "CHILDREN PREVIEW (" << area << "):\n"; for (const auto& r : rows) std::cout << "  " << r.line << "\n"; }
        } catch (...) { std::cout << "CHILDREN PREVIEW: (unavailable)\n"; }
        return {PagerAction::Repaint, false};
    }

    if (u.rfind("SHOW CHILDREN", 0) == 0) {
        std::cout << "CHILDREN (by relation):\n";
        const auto stats = relations_status::relation_stats_for_current_parent();
        if (stats.empty()) std::cout << "  (none)\n";
        else for (const auto& s : stats) std::cout << "  " << s.child_area << " — matches: " << s.match_count << "\n";
        return {PagerAction::Repaint, false};
    }

    if (u == "TOP") { stream.top(); return {PagerAction::Repaint, true}; }
    if (u == "BOTTOM") { stream.bottom(); return {PagerAction::Repaint, true}; }
    if (u.rfind("SKIP ", 0) == 0) { try { long n = std::stol(trim_copy(raw.substr(5))); stream.skip(n); } catch (...) {} return {PagerAction::Repaint, true}; }
    if (u.rfind("GOTO ", 0) == 0) { try { long n = std::stol(trim_copy(raw.substr(5))); (void)(stream.is_ordered() ? stream.goto_pos(n) : stream.goto_recno(n)); } catch (...) {} return {PagerAction::Repaint, true}; }

    if (u.rfind("STATUS ", 0) == 0) {
        const std::string mode = up(trim_copy(u.substr(7)));
        ps.status_verbose = (mode == "VERBOSE");
        return {PagerAction::Repaint, false};
    }

    std::cout << "(hint: OPEN CHILD/BACK/SHOW BREADCRUMBS/SHOW CHILDREN/SHOW CHILDREN FOR ... PREVIEW n/FOR/CLEAR FOR/SPEC ... )\n";
    return {PagerAction::Repaint, false};
}

static void run_smart_browser(std::istringstream& iss) {
    std::string spec = "*"; int page_size = 20; std::string for_expr; PagerState ps;

    std::string rest; std::getline(iss, rest);
    auto upper = rest; for (auto& c : upper) c = (char)std::toupper((unsigned char)c);
    auto pos_for = upper.find(" FOR ");
    if (pos_for != std::string::npos) { for_expr = rest.substr(pos_for + 5); rest.erase(pos_for); upper.erase(pos_for); }
    std::istringstream ss(rest);
    std::string tok; bool spec_set=false;
    while (ss >> tok) {
        std::string U = tok; for (auto& c : U) c=(char)std::toupper((unsigned char)c);
        if (U == "PAGESIZE") { ss >> page_size; continue; }
        if (U == "SHOW") { std::string w; ss >> w; std::string W=w; for(auto& c:W)c=(char)std::toupper((unsigned char)c); if(W=="SCHEMA") ps.show_schema=true; else if(W=="JSON") ps.show_json=true; continue; }
        if (U == "STATUS") { std::string w; ss >> w; std::string W=w; for(auto& c:W)c=(char)std::toupper((unsigned char)c); ps.status_verbose=(W=="VERBOSE"); continue; }
        if (!spec_set) { spec_set=true; std::string tail; std::getline(ss, tail); spec = trim(tok + tail); break; }
    }
    if (page_size <= 0) page_size = 20; if (page_size > 200) page_size = 200;

    dottalk::DbTupleStream stream(spec, "");
    if (!for_expr.empty()) stream.set_filter_for(for_expr);
    stream.top();

    const auto schema  = dottalk::SchemaResolver::resolve(stream.current_area_name());
    const auto sidecar = dottalk::SidecarLoader::load_json_sidecar(stream.current_area_name());

    std::vector<StreamCtx> breadcrumbs;
    StreamCtx cur_ctx{spec, for_expr};

    for (;;) {
        auto page = stream.next_page(static_cast<std::size_t>(page_size));
        if (page.empty()) std::cout << "(end)\n";
        else for (const auto& r : page) print_tuple_row(r);

        if (ps.show_schema) {
            std::cout << "Schema:\n";
            if (schema.fields.empty()) std::cout << "  (no logical schema)\n";
            else {
                std::cout << "  Source: " << schema.source << "\n  Order: ";
                for (size_t i=0;i<schema.fields.size();++i){ if(i) std::cout << " -> "; std::cout << schema.fields[i].name; }
                std::cout << "\n";
            }
        }
        if (ps.show_json) {
            std::cout << "JSON/sidecar:\n";
            if (!sidecar.has_value()) std::cout << "  (none)\n";
            else std::cout << "  " << *sidecar << "\n";
        }

        std::cout << stream.status_line() << "\n";
        auto pr = dispatch(stream, ps, breadcrumbs, cur_ctx);
        if (pr.action == PagerAction::Quit) break;
    }
}

} // namespace

void cmd_SMART_BROWSER(xbase::DbArea& /*A*/, std::istringstream& iss)
{
    WorkAreaCursorRestore __restore; // keep SMARTBROWSER read-only w.r.t. workarea cursors
    run_smart_browser(iss);
}

void cmd_SMARTBROWSER(xbase::DbArea& A, std::istringstream& iss) { cmd_SMART_BROWSER(A, iss); }

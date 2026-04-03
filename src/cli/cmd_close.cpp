// src/cli/cmd_close.cpp
#include <sstream>
#include <iostream>
#include <string>
#include <filesystem>
#include <cctype>
#include <vector>

#include "xbase.hpp"
#include "cli/dirty_prompt.hpp"
#include "cli/order_state.hpp"   // orderstate::clearOrder
#include "memo/memo_auto.hpp"     // cli_memo::memo_auto_on_close
#include "cli/table_state.hpp"   // dottalk::table::set_enabled/dirty/stale

#if __has_include("set_relations.hpp")
  #include "set_relations.hpp"   // relations_api::export_relations / import_relations / clear_all_relations
  #define HAVE_RELATIONS 1
#else
  #define HAVE_RELATIONS 0
#endif

namespace fs = std::filesystem;

// Provided by the interactive shell.
extern "C" xbase::XBaseEngine* shell_engine(void);

namespace {

static int area_index_from_ref(xbase::DbArea& areaRef) {
    xbase::XBaseEngine* eng = nullptr;
    try { eng = shell_engine(); } catch (...) { eng = nullptr; }
    if (!eng) return -1;

    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        try {
            if (&(eng->area(i)) == &areaRef) return i;
        } catch (...) {
            // ignore and keep scanning
        }
    }
    return -1;
}

static inline void clear_table_slot_state(xbase::DbArea& areaRef) {
    const int slot = area_index_from_ref(areaRef);
    if (slot < 0) return;

    dottalk::table::set_enabled(slot, false);
    dottalk::table::set_dirty(slot, false);
    dottalk::table::set_stale(slot, false);
}

} // namespace

#if HAVE_RELATIONS
static inline std::string up_copy(std::string s)
{
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static std::string close_table_key(const xbase::DbArea& a)
{
    // Prefer logical name because REL commands use logical names.
    std::string ln = a.logicalName();
    if (!ln.empty()) return ln;

    // Fallback: legacy name() if present/used as alias in your build.
    try {
        std::string nm = a.name();
        if (!nm.empty()) return nm;
    } catch (...) {}

    // Final fallback: stem of filename
    try {
        fs::path p(a.filename());
        if (!p.empty()) return p.stem().string();
    } catch (...) {}

    return {};
}

static void clear_relations_involving_table(const xbase::DbArea& a)
{
    const std::string key = up_copy(close_table_key(a));
    if (key.empty()) return;

    const auto all = relations_api::export_relations();

    std::vector<relations_api::RelationSpec> keep;
    keep.reserve(all.size());

    for (const auto& r : all) {
        const std::string p = up_copy(r.parent);
        const std::string c = up_copy(r.child);

        if (p == key || c == key) continue;
        keep.push_back(r);
    }

    if (keep.size() == all.size()) return;

    relations_api::import_relations(keep, /*clear_existing=*/true);

    try {
        const std::string cur = up_copy(relations_api::current_parent_name());
        if (cur == key) relations_api::set_current_parent_name(std::string{});
    } catch (...) {}
}
#endif // HAVE_RELATIONS

void cmd_CLOSE(xbase::DbArea& a, std::istringstream& iss)
{
    // Table buffering prompt gate
    if (!dottalk::dirty::maybe_prompt_area(a, "CLOSE")) {
        std::cout << "CLOSE canceled.\n";
        return;
    }

    // CLOSE currently doesn't really take args in your CLI usage, but we parse one
    // token anyway so "CLOSE ALL" can be added later without changing signature.
    std::string arg;
    iss >> arg;

#if HAVE_RELATIONS
    if (!arg.empty() && up_copy(arg) == "ALL") {
        relations_api::clear_all_relations();
    } else {
        clear_relations_involving_table(a);
    }
#endif

    // Memo sidecar lifecycle hook (must run even if no indexes)
    cli_memo::memo_auto_on_close(a);

    // Detach any order/index state (best effort)
    try { orderstate::clearOrder(a); } catch (...) {}

    // Close the work area
    try { a.close(); } catch (...) {}

    // Keep "filename() is open truth" consistent across commands.
    try { a.setFilename(""); } catch (...) {}

    // RULE: closing a DBF resets TABLE state for that slot (OFF + clean + fresh)
    clear_table_slot_state(a);

    std::cout << "Closed.\n";
}
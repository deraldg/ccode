// src/cli/set_relations.cpp
#include "set_relations.hpp"

#include "xbase.hpp"
#include "workareas.hpp"
#include "xbase_field_getters.hpp"
#include "db_tuple_stream.hpp"
#include "tuple_types.hpp"
#include "textio.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

// Forward declaration
static std::string get_by_index_as_string(xbase::DbArea& db, int field_index_1based);

// ---------------------------
// Internal structures
// ---------------------------

struct JoinField {
    int parent_field; // 1-based
    int child_field;  // 1-based
};

struct Relation {
    std::string child;                 // normalized (UPPER)
    std::vector<JoinField> joins;      // HARD bindings (indices)
    std::vector<std::string> names;    // original field names (for save/export)
};

std::unordered_map<std::string, std::vector<Relation>> g_relations; // key: parent (UPPER)

bool        g_autorefresh = true;
bool        g_verbose     = true;
std::size_t g_scan_limit  = 500000;

xbase::XBaseEngine* g_engine = nullptr;

// Optional override anchor. IMPORTANT:
// - We DO NOT auto-set this during ADD.
// - If empty, we always anchor to the CURRENT workarea (selected area).
std::string g_current_parent_name;

// ---------------------------
// Helpers
// ---------------------------

static std::string up_copy(std::string s) { return textio::up(std::move(s)); }

static std::string infer_parent_from_workarea() {
    try {
        const xbase::DbArea* A = workareas::at(workareas::current_slot());
        if (!A) return {};
        const std::string ln = A->logicalName();
        if (!ln.empty()) return up_copy(ln);
        return up_copy(A->name());
    } catch (...) { return {}; }
}

static std::string naked_field(std::string s) {
    auto dot = s.find('.');
    if (dot != std::string::npos) s = s.substr(dot + 1);
    return textio::trim(std::move(s));
}

static std::string ltrim_copy(std::string s) {
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    if (i) s.erase(0, i);
    return s;
}

static bool is_numeric_literal(const std::string& s) {
    const std::string t = ltrim_copy(textio::trim(s));
    if (t.empty()) return false;
    std::size_t i = 0;
    if (t[i] == '+' || t[i] == '-') ++i;
    bool any_digit = false;
    bool any_dot = false;
    for (; i < t.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(t[i]);
        if (std::isdigit(ch)) { any_digit = true; continue; }
        if (t[i] == '.' && !any_dot) { any_dot = true; continue; }
        return false;
    }
    return any_digit;
}

static xbase::DbArea* find_open_area_by_name_ci(const std::string& logical_or_name) {
    const std::string target = up_copy(textio::trim(logical_or_name));
    if (target.empty()) return nullptr;

    const std::size_t n = workareas::count();
    for (std::size_t i = 0; i < n; ++i) {
        xbase::DbArea* a = workareas::at(i);
        if (!a) continue;
        bool open = false;
        try { open = a->isOpen(); } catch (...) { open = false; }
        if (!open) continue;

        try {
            const std::string ln = a->logicalName();
            if (!ln.empty() && up_copy(ln) == target) return a;
            const std::string nm = a->name();
            if (!nm.empty() && up_copy(nm) == target) return a;
        } catch (...) {}
    }
    return nullptr;
}

static int slot_of_area_ptr(const xbase::DbArea* area) {
    if (!area) return -1;
    const std::size_t n = workareas::count();
    for (std::size_t i = 0; i < n; ++i) {
        if (workareas::at(i) == area) return static_cast<int>(i);
    }
    return -1;
}

class ScopedEngineSelect {
public:
    explicit ScopedEngineSelect(const xbase::DbArea* area) noexcept {
        if (!g_engine || !area) return;
        const int slot = slot_of_area_ptr(area);
        if (slot < 0) return;
        try {
            prev_ = g_engine->currentArea();
            if (prev_ != slot) {
                g_engine->selectArea(slot);
                active_ = true;
            }
        } catch (...) { active_ = false; }
    }

    ~ScopedEngineSelect() noexcept {
        if (!active_ || !g_engine) return;
        try { g_engine->selectArea(prev_); } catch (...) {}
    }

    ScopedEngineSelect(const ScopedEngineSelect&) = delete;
    ScopedEngineSelect& operator=(const ScopedEngineSelect&) = delete;

private:
    int prev_{-1};
    bool active_{false};
};

// ---------------------------------------------------------------------------
// Work-area state preservation
//
// REL inspection/enumeration commands should not change the user's current
// work-area selection or record pointers. During enumeration we necessarily
// reposition areas (TOP/SKIP/GOTO) to follow relations. This helper snapshots
// all open areas' record numbers and restores them on scope exit (best-effort).
// ---------------------------------------------------------------------------
class WorkAreaStateGuard {
public:
    explicit WorkAreaStateGuard() noexcept
        : eng_(g_engine)
        , prev_area_(g_engine ? g_engine->currentArea() : -1)
    {
        const std::size_t n = workareas::count();
        saved_.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            xbase::DbArea* a = workareas::at(i);
            if (!a || !a->isOpen()) continue;
            Saved s{};
            s.slot  = static_cast<int>(i);
            s.recno = a->recno();
            saved_.push_back(s);
        }
    }

    ~WorkAreaStateGuard() noexcept {
        // Restore record pointers first...
        for (const auto& s : saved_) {
            xbase::DbArea* a = workareas::at(static_cast<std::size_t>(s.slot));
            if (!a || !a->isOpen()) continue;
            try {
                const int32_t total = a->recCount();
                if (s.recno >= 1 && s.recno <= total) {
                    (void)a->gotoRec(s.recno);
                    (void)a->readCurrent();
                } else if (total > 0) {
                    a->top();
                    (void)a->readCurrent();
                }
            } catch (...) {
                // best-effort
            }
        }

        // ...then restore user's current area selection.
        if (eng_ && prev_area_ >= 0) {
            try { eng_->selectArea(prev_area_); } catch (...) {}
        }

        // If live-follow is enabled, re-sync children to the restored parent.
        // (No-op when follow is off.)
        try { relations_api::refresh_if_enabled(); } catch (...) {}
    }

    WorkAreaStateGuard(const WorkAreaStateGuard&) = delete;
    WorkAreaStateGuard& operator=(const WorkAreaStateGuard&) = delete;

private:
    struct Saved {
        int slot{-1};
        int32_t recno{0};
    };

    xbase::XBaseEngine* eng_{nullptr};
    int prev_area_{-1};
    std::vector<Saved> saved_{};
};

static int find_field_index_ci(const xbase::DbArea& db, const std::string& name) {
    const std::string target = up_copy(textio::trim(name));
    int idx = 1;
    for (const auto& fd : db.fields()) {
        if (up_copy(textio::trim(fd.name)) == target) return idx;
        ++idx;
    }
    return -1;
}

static const xbase::FieldDef* find_field_def_ci(const xbase::DbArea& db, const std::string& name) {
    const std::string target = up_copy(textio::trim(name));
    for (const auto& fd : db.fields()) {
        if (up_copy(textio::trim(fd.name)) == target) return &fd;
    }
    return nullptr;
}

static std::string get_by_index_as_string(xbase::DbArea& db, int field_index_1based) {
    if (field_index_1based <= 0) return {};
    const auto& fds = db.fields();
    const std::size_t idx0 = static_cast<std::size_t>(field_index_1based - 1);
    if (idx0 >= fds.size()) return {};
    ScopedEngineSelect focus(&db);
    return xfg::getFieldAsString(db, fds[idx0].name);
}

// kv for scan: (child_field_index, expected_value)
static std::vector<std::pair<int, std::string>>
parent_values(const xbase::DbArea& parent, const std::vector<JoinField>& joins) {
    std::vector<std::pair<int, std::string>> out;
    out.reserve(joins.size());
    xbase::DbArea& P = const_cast<xbase::DbArea&>(parent);
    for (const auto& j : joins) {
        out.emplace_back(j.child_field, get_by_index_as_string(P, j.parent_field));
    }
    return out;
}

// Numeric compare when both parse as numeric.
static bool values_match(xbase::DbArea& child,
                         const std::vector<std::pair<int, std::string>>& kv) {
    for (const auto& [child_field_index, expected_raw] : kv) {
        const std::string expected = textio::trim(expected_raw);

        const auto& fds = child.fields();
        const std::size_t idx0 = static_cast<std::size_t>(child_field_index - 1);
        const xbase::FieldDef* fd = (child_field_index > 0 && idx0 < fds.size()) ? &fds[idx0] : nullptr;

        const std::string actual = textio::trim(get_by_index_as_string(child, child_field_index));

        if (fd && fd->type == 'N' && !expected.empty() && !actual.empty()
            && is_numeric_literal(expected) && is_numeric_literal(actual)) {
            try {
                const double e = std::stod(expected);
                const double a = std::stod(actual);
                if (e != a) return false;
            } catch (...) {
                if (actual != expected) return false;
            }
        } else {
            if (actual != expected) return false;
        }
    }
    return true;
}

static bool goto_first_match(xbase::DbArea& child,
                             const std::vector<std::pair<int, std::string>>& kv,
                             std::size_t scan_limit) {
    if (!child.isOpen()) return false;

    const int32_t rec_count = child.recCount();
    if (rec_count <= 0) return false;

    {
        ScopedEngineSelect focus(&child);
        if (!child.top()) return false;
        try { if (!child.readCurrent()) return false; } catch (...) { return false; }
    }

    if (child.recno() <= 0 || child.recno() > rec_count) return false;

    std::size_t scanned = 0;
    for (;;) {
        {
            ScopedEngineSelect focus(&child);
            try { child.readCurrent(); } catch (...) {}
            if (!child.isDeleted() && values_match(child, kv)) return true;

            if (++scanned >= scan_limit) return false;

            const int prev = child.recno();
            if (!child.skip(1)) return false;
            const int next = child.recno();
            if (next <= prev) return false;
            if (next > rec_count) return false;
            try { if (!child.readCurrent()) return false; } catch (...) { return false; }
            const int post = child.recno();
            if (post <= prev) return false;
            if (post > rec_count) return false;
        }
    }
}

static void clear_subtree_to_top(const std::string& parent_name,
                                 std::unordered_set<std::string>& seen,
                                 int depth) {
    if (depth > 24) return;
    const std::string key = up_copy(parent_name);
    if (!seen.insert(key).second) return;

    auto it = g_relations.find(key);
    if (it == g_relations.end()) return;

    for (const auto& r : it->second) {
        if (xbase::DbArea* child = find_open_area_by_name_ci(r.child)) {
            try { child->top(); child->readCurrent(); } catch (...) {}
        }
        clear_subtree_to_top(r.child, seen, depth + 1);
    }
}

static void refresh_from_parent_name(const std::string& parent_name,
                                     std::unordered_set<std::string>& seen,
                                     int depth) {
    if (depth > 24) return;
    const std::string key = up_copy(parent_name);
    if (!seen.insert(key).second) return;

    xbase::DbArea* parent = find_open_area_by_name_ci(parent_name);
    if (!parent || parent->recno() <= 0) return;

    try { parent->readCurrent(); } catch (...) {}

    auto it = g_relations.find(key);
    if (it == g_relations.end()) return;

    for (const auto& rel : it->second) {
        xbase::DbArea* child = find_open_area_by_name_ci(rel.child);
        if (!child) continue;

        const auto kv = parent_values(*parent, rel.joins);

        bool found = false;
        try { found = goto_first_match(*child, kv, g_scan_limit); } catch (...) { found = false; }

        if (!found) {
            try { child->top(); child->readCurrent(); } catch (...) {}
            std::unordered_set<std::string> sub_seen;
            clear_subtree_to_top(rel.child, sub_seen, depth + 1);
            continue;
        }

        refresh_from_parent_name(rel.child, seen, depth + 1);
    }
}

// Build FOR expr remains name-based (DbTupleStream uses expressions).
static std::string build_for_expr(const xbase::DbArea* child_db,
                                  const std::vector<std::pair<std::string, std::string>>& kv) {
    std::string expr;
    for (std::size_t i = 0; i < kv.size(); ++i) {
        if (i) expr += " AND ";
        expr += kv[i].first;
        expr += " = ";

        const std::string val = textio::trim(kv[i].second);
        const xbase::FieldDef* fd = child_db ? find_field_def_ci(*child_db, kv[i].first) : nullptr;
        const bool numeric_field = fd && fd->type == 'N';

        if (numeric_field && !val.empty() && is_numeric_literal(val)) {
            expr += val;
        } else {
            expr.push_back('"');
            for (char c : val) {
                if (c == '"') expr += "\"\"";
                else expr.push_back(c);
            }
            expr.push_back('"');
        }
    }
    return expr;
}

// Used ONLY for match_count / preview: derive parent kv as (field_name, value) from HARD bindings.
static std::vector<std::pair<std::string, std::string>>
parent_field_values_names(const xbase::DbArea& parent, const Relation& rel) {
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(rel.joins.size());
    xbase::DbArea& P = const_cast<xbase::DbArea&>(parent);

    for (std::size_t i = 0; i < rel.joins.size() && i < rel.names.size(); ++i) {
        const std::string field_name = naked_field(rel.names[i]);
        const std::string val = get_by_index_as_string(P, rel.joins[i].parent_field);
        out.emplace_back(field_name, val);
    }
    return out;
}

} // namespace

// ==============================
// Public API
// ==============================

namespace relations_api {

void attach_engine(xbase::XBaseEngine* eng) noexcept { g_engine = eng; }

void set_autorefresh(bool on) noexcept { g_autorefresh = on; }
void set_verbose(bool on) noexcept { g_verbose = on; }
void set_scan_limit(std::size_t max_steps) noexcept { g_scan_limit = max_steps ? max_steps : 1; }

bool add_relation(const std::string& parent_area,
                  const std::string& child_area,
                  const std::vector<std::string>& tuple_fields) {
    const std::string parent = up_copy(parent_area);
    const std::string child  = up_copy(child_area);

    xbase::DbArea* P = find_open_area_by_name_ci(parent);
    xbase::DbArea* C = find_open_area_by_name_ci(child);
    if (!P || !C) {
        if (g_verbose) std::cout << "REL: add failed (parent/child not open)\n";
        return false;
    }

    Relation r;
    r.child = child;
    r.names = tuple_fields;
    r.joins.clear();
    r.joins.reserve(tuple_fields.size());

    for (const auto& f_raw : tuple_fields) {
        const std::string f = naked_field(f_raw);
        const int pf = find_field_index_ci(*P, f);
        const int cf = find_field_index_ci(*C, f);
        if (pf <= 0 || cf <= 0) {
            std::cout << "REL: field not found: " << f << "\n";
            return false;
        }
        r.joins.push_back(JoinField{pf, cf});
    }

    auto& v = g_relations[parent];
    auto it = std::find_if(v.begin(), v.end(), [&](const Relation& x){ return x.child == child; });
    if (it == v.end()) v.push_back(std::move(r));
    else *it = std::move(r);

    if (g_verbose) {
        std::cout << "REL: " << parent << " -> " << child << " ON ";
        for (std::size_t i = 0; i < tuple_fields.size(); ++i) {
            if (i) std::cout << ",";
            std::cout << tuple_fields[i];
        }
        std::cout << "\n";
    }

    return true;
}

bool remove_relation(const std::string& parent_area,
                     const std::string& child_area) {
    const std::string parent = up_copy(parent_area);
    const std::string child  = up_copy(child_area);

    auto it = g_relations.find(parent);
    if (it == g_relations.end()) {
        if (g_verbose) {
            std::cout << "REL: no relations defined for " << parent << "\n";
        }
        return false;
    }

    auto& vec = it->second;
    const auto old_size = vec.size();

    vec.erase(
        std::remove_if(vec.begin(), vec.end(),
            [&](const Relation& rel) {
                return up_copy(rel.child) == child;
            }),
        vec.end());

    if (vec.size() == old_size) {
        if (g_verbose) {
            std::cout << "REL: relation not found: " << parent << " -> " << child << "\n";
        }
        return false;
    }

    if (vec.empty()) {
        g_relations.erase(it);
    }

    if (g_verbose) {
        std::cout << "REL: removed " << parent << " -> " << child << "\n";
    }

    return true;
}

void clear_relations(const std::string& parent_area) {
    g_relations.erase(up_copy(parent_area));
    if (g_verbose) std::cout << "REL: cleared for " << up_copy(parent_area) << "\n";
}

void clear_all_relations() {
    g_relations.clear();
    g_current_parent_name.clear();
    if (g_verbose) std::cout << "REL: cleared all\n";
}

void set_current_parent_name(const std::string& logical_name) noexcept {
    g_current_parent_name = up_copy(logical_name);
}

std::string current_parent_name() {
    if (!g_current_parent_name.empty()) {
        if (find_open_area_by_name_ci(g_current_parent_name)) return g_current_parent_name;
        g_current_parent_name.clear();
    }
    return infer_parent_from_workarea();
}

void refresh_for_current_parent() noexcept {
    try {
        const std::string parent = current_parent_name();
        if (parent.empty()) return;
        std::unordered_set<std::string> seen;
        refresh_from_parent_name(parent, seen, 0);
    } catch (...) {}
}

void refresh_if_enabled() noexcept { if (g_autorefresh) refresh_for_current_parent(); }

std::vector<std::string> child_areas_for_current_parent() {
    std::vector<std::string> out;
    const auto parent = current_parent_name();
    if (parent.empty()) return out;

    auto it = g_relations.find(up_copy(parent));
    if (it == g_relations.end()) return out;

    out.reserve(it->second.size());
    for (const auto& r : it->second) out.push_back(r.child);
    return out;
}

int match_count_for_child(const std::string& child_area) {
    try {
        const std::string parent = current_parent_name();
        if (parent.empty()) return 0;

        xbase::DbArea* parent_db = find_open_area_by_name_ci(parent);
        if (!parent_db || parent_db->recno() <= 0) return 0;

        auto it = g_relations.find(up_copy(parent));
        if (it == g_relations.end()) return 0;

        const std::string child = up_copy(child_area);
        auto rit = std::find_if(it->second.begin(), it->second.end(),
                                [&](const Relation& r){ return r.child == child; });
        if (rit == it->second.end()) return 0;

        xbase::DbArea* child_db = find_open_area_by_name_ci(child);
        if (!child_db || !child_db->isOpen()) return 0;

        const int parent_recno = parent_db->recno();
        const int child_start_recno = child_db->recno();

        {
            ScopedEngineSelect focus(parent_db);
            try { parent_db->readCurrent(); } catch (...) {}
        }
        const auto kv = parent_values(*parent_db, rit->joins);

        const int32_t rec_count = child_db->recCount();
        if (rec_count <= 0) return 0;

        int count = 0;
        std::size_t scanned = 0;

        {
            ScopedEngineSelect focus(child_db);
            try { child_db->top(); } catch (...) {}
            try { child_db->readCurrent(); } catch (...) {}

            for (;;) {
                const int cur = child_db->recno();
                if (cur <= 0 || cur > rec_count) break;

                try { child_db->readCurrent(); } catch (...) {}
                if (!child_db->isDeleted() && values_match(*child_db, kv)) ++count;

                if (++scanned >= g_scan_limit) break;

                const int prev = child_db->recno();
                if (!child_db->skip(1)) break;
                const int next = child_db->recno();
                if (next <= prev) break;
                if (next > rec_count) break;
                try { if (!child_db->readCurrent()) break; } catch (...) { break; }
                const int post = child_db->recno();
                if (post <= prev) break;
                if (post > rec_count) break;
            }

            try {
                if (child_start_recno > 0 && child_start_recno <= rec_count) {
                    child_db->gotoRec(child_start_recno);
                    child_db->readCurrent();
                }
            } catch (...) {}
        }

        {
            ScopedEngineSelect focus(parent_db);
            try {
                if (parent_recno > 0) {
                    parent_db->gotoRec(parent_recno);
                    parent_db->readCurrent();
                }
            } catch (...) {}
        }

        return count;
    } catch (...) { return 0; }
}

std::vector<RelationSpec> export_relations() {
    std::vector<RelationSpec> out;
    for (const auto& kv : g_relations) {
        const std::string& parent = kv.first;
        for (const auto& rel : kv.second) {
            out.push_back(RelationSpec{ parent, rel.child, rel.names });
        }
    }
    return out;
}

void import_relations(const std::vector<RelationSpec>& specs, bool clear_existing) {
    if (clear_existing) g_relations.clear();
    for (const auto& s : specs) add_relation(s.parent, s.child, s.fields);
}

// ---- Accurate REL LIST ALL support helpers (internal) ----

static std::unordered_map<std::string, xbase::DbArea*> build_area_by_up_name() {
    std::unordered_map<std::string, xbase::DbArea*> out;

    const std::size_t n = workareas::count();
    out.reserve(n * 2);

    for (std::size_t i = 0; i < n; ++i) {
        xbase::DbArea* a = workareas::at(i);
        if (!a || !a->isOpen()) continue;

        std::string ln;
        try { ln = a->logicalName(); } catch (...) {}
        const std::string key = up_copy(!ln.empty() ? ln : a->name());
        if (!key.empty()) out[key] = a;
    }

    return out;
}

static std::vector<std::string> infer_unique_child_chain(const std::string& root_up, int max_depth) {
    std::vector<std::string> chain;
    if (max_depth <= 0) return chain;

    std::unordered_set<std::string> seen;
    seen.insert(root_up);

    std::string cur = root_up;
    for (int depth = 1; depth <= max_depth; ++depth) {
        auto it = g_relations.find(cur);
        if (it == g_relations.end() || it->second.empty()) break;

        if (it->second.size() != 1) break;

        const std::string child = up_copy(it->second.front().child);
        if (child.empty() || seen.count(child)) break;

        chain.push_back(child);
        seen.insert(child);
        cur = child;
    }

    return chain;
}

static std::string format_on_fields(const Relation& rel) {
    std::string s;
    if (!rel.names.empty()) {
        s += " ON ";
        for (std::size_t i = 0; i < rel.names.size(); ++i) {
            if (i) s += ",";
            s += rel.names[i];
        }
    }
    return s;
}

// ---- SURGICALLY REPLACED: list_tree_for_current_parent ----

std::vector<PreviewRow> list_tree_for_current_parent(bool recursive, int max_depth) {
    std::vector<PreviewRow> out;

    const std::string root = up_copy(current_parent_name());
    if (root.empty()) return out;

    out.push_back(PreviewRow{root});

    if (!recursive || max_depth <= 0) {
        auto it = g_relations.find(root);
        if (it == g_relations.end()) return out;

        for (const auto& rel : it->second) {
            std::string line(2u, ' ');
            line += "-> ";
            line += rel.child;
            line += format_on_fields(rel);

            int cnt = 0;
            try { cnt = match_count_for_child(rel.child); } catch (...) {}
            line += "  (matches: ";
            line += std::to_string(cnt);
            line += ")";

            out.push_back(PreviewRow{std::move(line)});
        }
        return out;
    }

    const auto area_by = build_area_by_up_name();
    const auto chain_children = infer_unique_child_chain(root, max_depth);

    std::vector<std::string> chain_names;
    chain_names.reserve(1 + chain_children.size());
    chain_names.push_back(root);
    for (const auto& c : chain_children) chain_names.push_back(c);

    std::unordered_map<std::string, std::unordered_set<int32_t>> distinct_recnos;
    distinct_recnos.reserve(chain_names.size() * 2);

    if (auto itA = area_by.find(root); itA != area_by.end() && itA->second && itA->second->isOpen()) {
        distinct_recnos[root].insert(itA->second->recno());
    }

    (void)enum_emit_for_current_parent(
        chain_children,
        0,
        [&] {
            for (const auto& nm : chain_names) {
                auto it = area_by.find(nm);
                if (it == area_by.end()) continue;
                xbase::DbArea* a = it->second;
                if (!a || !a->isOpen()) continue;
                distinct_recnos[nm].insert(a->recno());
            }
        },
        nullptr
    );

    auto get_distinct_count = [&](const std::string& up_name) -> int {
        auto it = distinct_recnos.find(up_name);
        if (it == distinct_recnos.end()) return -1;
        return static_cast<int>(it->second.size());
    };

    std::unordered_set<std::string> path;
    path.insert(root);

    std::function<void(const std::string&, int)> dfs;
    dfs = [&](const std::string& parent_up, int depth) {
        if (depth > max_depth) return;

        auto it = g_relations.find(parent_up);
        if (it == g_relations.end() || it->second.empty()) return;

        for (const auto& rel : it->second) {
            const std::string child_up = up_copy(rel.child);

            std::string line(static_cast<std::size_t>(depth) * 2u, ' ');
            line += "-> ";
            line += rel.child;
            line += format_on_fields(rel);

            int cnt = get_distinct_count(child_up);
            if (cnt >= 0) {
                line += "  (matches: ";
                line += std::to_string(cnt);
                line += ")";
            } else if (depth == 1) {
                int c1 = 0;
                try { c1 = match_count_for_child(rel.child); } catch (...) {}
                line += "  (matches: ";
                line += std::to_string(c1);
                line += ")";
            } else {
                line += "  (matches: n/a)";
            }

            out.push_back(PreviewRow{std::move(line)});

            if (child_up.empty() || path.count(child_up)) continue;
            path.insert(child_up);
            dfs(child_up, depth + 1);
            path.erase(child_up);
        }
    };

    dfs(root, 1);
    return out;
}

std::vector<relations_api::PreviewRow> preview_child(const std::string& child_area, int limit) {
    if (limit <= 0) return {};
    std::vector<PreviewRow> out;
    try {
        const auto parent = current_parent_name();
        if (parent.empty()) return out;

        const xbase::DbArea* A = workareas::at(workareas::current_slot());
        if (!A || A->recno() <= 0) return out;

        try { const_cast<xbase::DbArea*>(A)->readCurrent(); } catch (...) {}

        auto it = g_relations.find(up_copy(parent));
        if (it == g_relations.end()) return out;

        const std::string child = up_copy(child_area);
        auto rit = std::find_if(it->second.begin(), it->second.end(),
                                [&](const Relation& r){ return r.child == child; });
        if (rit == it->second.end()) return out;

        const xbase::DbArea* child_db = find_open_area_by_name_ci(child);

        auto kv = parent_field_values_names(*A, *rit);
        const std::string for_expr = build_for_expr(child_db, kv);

        dottalk::DbTupleStream ts(child + ".*", "");
        ts.set_filter_for(for_expr);
        ts.top();

        const auto tuples = ts.next_page(static_cast<std::size_t>(limit));
        for (const auto& tr : tuples) {
            std::string line;
            for (std::size_t i = 0; i < tr.columns.size() && i < tr.values.size(); ++i) {
                if (i) line += " | ";
                line += tr.columns[i].name;
                line += "=";
                line += tr.values[i].empty() ? "\"\"" : tr.values[i];
            }
            out.push_back(PreviewRow{ std::move(line) });
        }
    } catch (...) {}
    return out;
}

namespace {

static std::vector<std::string> infer_unique_chain_from_parent(const std::string& parent_up) {
    std::vector<std::string> chain;
    std::string cur = parent_up;
    for (int depth = 0; depth < 32; ++depth) {
        auto it = g_relations.find(cur);
        if (it == g_relations.end()) break;
        if (it->second.empty()) break;
        if (it->second.size() != 1) return {};
        const std::string& child = it->second[0].child;
        chain.push_back(child);
        cur = child;
    }
    return chain;
}

static bool enum_chain_dfs(
    const std::vector<std::string>& chain_children,
    std::size_t idx,
    const std::function<void()>& emit,
    std::size_t max_rows,
    std::size_t* emitted,
    const std::string& parent_up) {

    if (!emitted) return false;
    if (idx >= chain_children.size()) {
        emit();
        ++(*emitted);
        return true;
    }

    xbase::DbArea* parent_db = find_open_area_by_name_ci(parent_up);
    const std::string child_up = up_copy(chain_children[idx]);
    xbase::DbArea* child_db = find_open_area_by_name_ci(child_up);
    if (!parent_db || !child_db) return false;

    auto it = g_relations.find(parent_up);
    if (it == g_relations.end()) return false;
    auto rit = std::find_if(it->second.begin(), it->second.end(),
                            [&](const Relation& r){ return r.child == child_up; });
    if (rit == it->second.end()) return false;

    {
        ScopedEngineSelect focus(parent_db);
        try { parent_db->readCurrent(); } catch (...) {}
    }

    const auto kv = parent_values(*parent_db, rit->joins);

    const int32_t rec_count = child_db->recCount();
    if (rec_count <= 0) return true;

    {
        ScopedEngineSelect focus(child_db);
        try { child_db->top(); } catch (...) { return false; }
        try { child_db->readCurrent(); } catch (...) {}
    }

    if (child_db->recno() <= 0 || child_db->recno() > rec_count) return true;

    for (std::size_t scanned = 0; scanned < g_scan_limit; ++scanned) {
        ScopedEngineSelect focus(child_db);

        const int cur = child_db->recno();
        if (cur <= 0 || cur > rec_count) break;

        try { child_db->readCurrent(); } catch (...) {}

        const bool matched = (!child_db->isDeleted() && values_match(*child_db, kv));
        const int match_recno = child_db->recno();

        if (matched) {
            if (!enum_chain_dfs(chain_children, idx + 1, emit, max_rows, emitted, child_up)) {
                return false;
            }
            if (max_rows != 0 && *emitted >= max_rows) return true;

            try {
                child_db->gotoRec(match_recno);
                child_db->readCurrent();
            } catch (...) {}
        }

        const int prev_recno = child_db->recno();
        if (!child_db->skip(1)) break;

        const int next_recno = child_db->recno();
        if (next_recno <= prev_recno) break;
        if (next_recno > rec_count) break;

        try { if (!child_db->readCurrent()) break; } catch (...) { break; }
        const int post_read_recno = child_db->recno();
        if (post_read_recno <= prev_recno) break;
        if (post_read_recno > rec_count) break;
    }

    return true;
}

} // anonymous namespace

bool enum_emit_for_current_parent(const std::vector<std::string>& path_children,
                                  std::size_t max_rows,
                                  const std::function<void()>& emit,
                                  std::size_t* rows_emitted) {
    if (rows_emitted) *rows_emitted = 0;

    // Non-invasive: enumeration should not disturb current record pointers.
    // We snapshot all open work areas and restore them on exit.
    WorkAreaStateGuard preserve;

    try {
        const std::string parent = current_parent_name();
        if (parent.empty()) return false;

        const std::string parent_up = up_copy(parent);
        std::vector<std::string> chain = path_children;
        if (chain.empty()) {
            chain = infer_unique_chain_from_parent(parent_up);
            if (chain.empty()) {
                return false;
            }
        }

        std::size_t emitted = 0;
        if (!enum_chain_dfs(chain, 0, emit, max_rows, &emitted, parent_up)) {
            if (rows_emitted) *rows_emitted = emitted;
            return false;
        }
        if (rows_emitted) *rows_emitted = emitted;
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace relations_api
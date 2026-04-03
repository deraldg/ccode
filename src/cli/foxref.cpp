#include "foxref.hpp"

#include <algorithm>
#include <cctype>

namespace foxref {

static std::string upper(std::string_view sv) {
    std::string s(sv.begin(), sv.end());
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

const Item* find(std::string_view name) {
    const std::string key = upper(name);
    for (const auto& it : catalog()) {
        if (it.name && upper(it.name) == key) return &it;
    }
    return nullptr;
}

std::vector<const Item*> search(std::string_view token) {
    std::vector<const Item*> out;
    const std::string key = upper(token);
    for (const auto& it : catalog()) {
        const std::string n = it.name ? upper(it.name) : std::string{};
        const std::string syn = it.syntax ? upper(it.syntax) : std::string{};
        const std::string sum = it.summary ? upper(it.summary) : std::string{};
        if (n.find(key) != std::string::npos ||
            syn.find(key) != std::string::npos ||
            sum.find(key) != std::string::npos) {
            out.push_back(&it);
        }
    }
    return out;
}

// ---------------- Beta checklist (compile-time defaults) ---------------------

const std::vector<BetaItem>& beta_catalog() {
    static const std::vector<BetaItem> k = {
        // Build & Hygiene
        {"BETA-1.1", "Build", "Clean build from empty tree succeeds (MSVC/Release).",
            "Gate: configure + build from a clean directory, no manual steps.", BetaStatus::OPEN},
        {"BETA-1.2", "Build", "No reliance on case-insensitive paths (CMake/file names).",
            "Gate: case-correct filenames; Linux/WSL should not break due to casing.", BetaStatus::OPEN},
        {"BETA-1.3", "Build", "No placeholder sources compiled (no '...' stubs in build).",
            "Quarantine staged files or remove them from globbing/targets.", BetaStatus::OPEN},
        {"BETA-1.4", "Build", "CMake is deterministic (no accidental glob drift).",
            "Gate: explicit target_sources or restricted globs; avoid pulling experimental files.", BetaStatus::OPEN},
        {"BETA-1.5", "Build", "Warnings reviewed; no UB in core paths.",
            "You can allow some warnings, but they must be explained and tracked.", BetaStatus::OPEN},

        // Physical layer
        {"BETA-2.1", "Storage", "DBF open/close/navigation stable (TOP/BOTTOM/GOTO/SKIP).",
            "Gate: repeatable navigation results under PHYSICAL order.", BetaStatus::OPEN},
        {"BETA-2.2", "Storage", "Deleted record visibility/semantics frozen.",
            "Gate: DELETED() and ALL/DELETED switches behave consistently everywhere.", BetaStatus::OPEN},
        {"BETA-2.3", "Storage", "Memo sidecar read/write round-trip validated.",
            "Gate: memo write then re-open yields identical content.", BetaStatus::OPEN},
        {"BETA-2.4", "Storage", "INX attach/detach and ORDER INX behavior frozen.",
            "Gate: ORDER INX produces deterministic traversal.", BetaStatus::OPEN},
        {"BETA-2.5", "Storage", "CNX container attach and ORDER CNX behavior frozen.",
            "Gate: CNX tag selection and traversal are deterministic and reported clearly.", BetaStatus::OPEN},

        // Schema engine
        {"BETA-3.1", "Schema", "Schema resolution order frozen (DDL -> header -> fallback).",
            "Gate: the resolver is documented and consistent across tuple/browse/sb.", BetaStatus::OPEN},
        {"BETA-3.2", "Schema", "Ownership flags frozen (BASE/EXT/MEMO/DERIVED).",
            "Gate: output labels match documented meaning; no ad-hoc inference in UI.", BetaStatus::OPEN},
        {"BETA-3.3", "Schema", "SCHEMA VALIDATE behavior frozen.",
            "Gate: validator schema versioning is stable; clear errors.", BetaStatus::OPEN},
        {"BETA-3.4", "Schema", "SCHEMA CREATE DBF FROM schema.json frozen.",
            "Gate: OVERWRITE/SEED/REJECTS/EMIT SIDECARS documented and stable.", BetaStatus::OPEN},
        {"BETA-3.5", "Schema", "DDL sidecar format frozen.",
            "Gate: sidecar schema has a version and compatibility rules.", BetaStatus::OPEN},
        {"BETA-3.6", "Schema", "Type authority centralized (schema engine owns typing).",
            "Gate: no UI or command invents types differently than schema resolver.", BetaStatus::OPEN},

        // Tuples
        {"BETA-4.1", "Tuple", "Tuple definition frozen (one logical row; may span areas).",
            "Gate: tuple invariants documented; provenance always present.", BetaStatus::OPEN},
        {"BETA-4.2", "Tuple", "Provenance fragments contract frozen (area/recno/source_kind).",
            "Gate: fragments exist and are stable; primary fragment semantics defined.", BetaStatus::OPEN},
        {"BETA-4.3", "Tuple", "Tuple builder extracted as canonical API (cmd_TUPLE becomes thin wrapper).",
            "Gate: one code path builds tuples for SB, TUPLE, exports.", BetaStatus::OPEN},
        {"BETA-4.4", "Tuple", "Spec resolution deterministic (*, AREA.*, AREA.FIELD, #n).",
            "Gate: empty spec is a hard error; last-good spec optional but defined.", BetaStatus::OPEN},

        // Relations
        {"BETA-5.1", "Relations", "Relations are configuration (no implicit joins/flattening).",
            "Gate: relations never merge rows; they expose related sets.", BetaStatus::OPEN},
        {"BETA-5.2", "Relations", "REL ADD/CLEAR/LIST/REFRESH semantics frozen.",
            "Gate: behavior documented; errors are non-destructive.", BetaStatus::OPEN},
        {"BETA-5.3", "Relations", "Relations save/load format frozen and validated.",
            "Gate: persistence round-trip works; missing file handled gracefully.", BetaStatus::OPEN},
        {"BETA-5.4", "Relations", "SB consumes relation metadata only (no DBF scanning for relations).",
            "Gate: UI queries the relations engine; engine owns matching/probing.", BetaStatus::OPEN},

        // TupleStream / navigation
        {"BETA-6.1", "TupleStream", "TOP/BOTTOM/SKIP/GOTO semantics frozen for tuple iteration.",
            "Gate: behavior defined when tuples span multiple records/areas.", BetaStatus::OPEN},
        {"BETA-6.2", "TupleStream", "FOR filters evaluate on tuple values only.",
            "Gate: name resolution deterministic; no direct DbArea reads in filter layer.", BetaStatus::OPEN},
        {"BETA-6.3", "TupleStream", "Type semantics defined (numeric/date/string comparisons).",
            "Gate: bare numerics and YYYYMMDD behave as documented.", BetaStatus::OPEN},
        {"BETA-6.4", "TupleStream", "Paging deterministic; empty page behavior non-fatal and explained.",
            "Gate: SB never exits/crashes on end-of-stream; displays an explicit message.", BetaStatus::OPEN},

        // Super Browser v1 read-only
        {"BETA-7.1", "SuperBrowser", "Scope locked: read-only only (editing explicitly disabled).",
            "Gate: no hidden edit paths; future editing remains deferred by contract.", BetaStatus::OPEN},
        {"BETA-7.2", "SuperBrowser", "Pager model frozen (-- More -- Enter/Q; no nested shell).",
            "Gate: UI loop is predictable; no command processor inside SB.", BetaStatus::OPEN},
        {"BETA-7.3", "SuperBrowser", "Status footer accurate (area/order/filter/rel configured).",
            "Gate: status reflects actual state; not stale.", BetaStatus::OPEN},
        {"BETA-7.4", "SuperBrowser", "Child tuples panel exists (bounded, non-flattening).",
            "Gate: SHOW CHILDREN LIMIT n honors relations without flattening.", BetaStatus::OPEN},

        // Command surface stability
        {"BETA-8.1", "Commands", "USE/LIST/COUNT/SCAN semantics frozen (legacy layer).",
            "Gate: no breaking behavior changes without versioning.", BetaStatus::OPEN},
        {"BETA-8.2", "Commands", "SCHEMA/REL/TUPLE/SB semantics frozen (modern layer).",
            "Gate: help + docs match behavior; no silent fallback semantics.", BetaStatus::OPEN},
        {"BETA-8.3", "Commands", "Error behavior is explicit and non-destructive.",
            "Gate: failures never partially mutate state; messages guide user.", BetaStatus::OPEN},

        // Documentation
        {"BETA-9.1", "Docs", "Relations Engine manual complete and synced with behavior.",
            "Gate: docs match command output and flags.", BetaStatus::OPEN},
        {"BETA-9.2", "Docs", "Schema Engine manual complete and synced with behavior.",
            "Gate: schema JSON examples match validator.", BetaStatus::OPEN},
        {"BETA-9.3", "Docs", "Super Browser manual complete (read-only, tuple-first).",
            "Gate: explains spec/filter/order/children panels.", BetaStatus::OPEN},
        {"BETA-9.4", "Docs", "Known limitations explicitly documented (post-beta list).",
            "Gate: editing/perf/SQL federation are clearly out-of-scope.", BetaStatus::OPEN},

        // Tests (minimum)
        {"BETA-10.1", "Tests", "Smoke tests cover open/navigate/filter/rel/status.",
            "Gate: scripts or harness runs on a reference dataset.", BetaStatus::OPEN},
        {"BETA-10.2", "Tests", "Contract tests: tuple invariants + schema order + relation persistence.",
            "Gate: regression catches contract drift.", BetaStatus::OPEN},

        // Final gate
        {"BETA-11.1", "Release", "No architectural TODOs remain in beta-scope paths.",
            "Gate: staged work is quarantined; core is clean.", BetaStatus::OPEN},
        {"BETA-11.2", "Release", "Version tagged; beta declaration text prepared.",
            "Gate: reproducible build + release notes.", BetaStatus::OPEN},
    };
    return k;
}

const BetaItem* beta_find(std::string_view id) {
    const std::string key = upper(id);
    for (const auto& it : beta_catalog()) {
        if (it.id && upper(it.id) == key) return &it;
    }
    return nullptr;
}

std::vector<const BetaItem*> beta_by_area(std::string_view area) {
    std::vector<const BetaItem*> out;
    const std::string key = upper(area);
    for (const auto& it : beta_catalog()) {
        if (!it.area) continue;
        if (upper(it.area) == key) out.push_back(&it);
    }
    return out;
}

} // namespace foxref

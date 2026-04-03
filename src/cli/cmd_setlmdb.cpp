// src/cli/cmd_setlmdb.cpp
//
// SETLMDB: select LMDB-backed ordering (container/tag/asc/desc) per area.
//
// Ownership rule enforced here:
//   DbArea -> IndexManager -> CdxBackend -> LMDB env
// No global LMDB singleton is touched from this command.
//
// Behavior:
//   SETLMDB 0                      -> clear ordering (physical)
//   SETLMDB                        -> report state
//   SETLMDB <stem|cdx|envdir>      -> default tag selection
//   SETLMDB <...> <TAG> [--asc|--desc]
//
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

#include "xbase.hpp"
#include "xindex/index_manager.hpp"

#include "textio.hpp"
#include "cli/cmd_setpath.hpp"
#include "cli/order_state.hpp"

namespace {

static inline std::string trim_copy(const std::string& s) { return textio::trim(s); }

static inline std::string upper_copy(std::string s) {
    for (char& c : s) {
        const unsigned char uc = static_cast<unsigned char>(c);
        c = static_cast<char>(std::toupper(uc));
    }
    return s;
}

static inline bool ends_with_ci(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) return false;
    return upper_copy(s.substr(s.size() - suffix.size())) == upper_copy(suffix);
}

static inline bool contains_path_sep(const std::string& s) {
    return (s.find('/') != std::string::npos) || (s.find('\\') != std::string::npos);
}

static inline bool looks_absolute(const std::string& s) {
    return (s.size() > 2 && std::isalpha(static_cast<unsigned char>(s[0])) && s[1] == ':') ||
           (!s.empty() && (s[0] == '/' || s[0] == '\\'));
}

// Resolve user token to container "<INDEXES>/<STEM>.cdx" (stable contract).
static std::string resolve_container_path(const std::string& token) {
    const std::filesystem::path idx = dottalk::paths::get_slot(dottalk::paths::Slot::INDEXES);

    // If token is already an envdir (<something>.cdx.d), normalize to container by stripping ".d".
    if (ends_with_ci(token, ".cdx.d")) {
        const auto envdir = (contains_path_sep(token) || looks_absolute(token)) ? token : (idx / token).string();
        return envdir.substr(0, envdir.size() - 2);
    }

    // If token is a container "students.cdx"
    if (ends_with_ci(token, ".cdx")) {
        return (contains_path_sep(token) || looks_absolute(token)) ? token : (idx / token).string();
    }

    // Otherwise treat as stem ("students")
    std::string stem = token;
    if (stem.empty()) stem = "students";
    return (idx / (upper_copy(stem) + ".cdx")).string();
}

} // namespace

void cmd_SETLMDB(xbase::DbArea& db, std::istringstream& iss) {
    std::string tok;
    iss >> tok;
    tok = trim_copy(tok);

    // Report-only form
    if (tok.empty()) {
        if (!orderstate::hasOrder(db)) {
            std::cout << "SETLMDB: none (physical order).\n";
            return;
        }
        std::cout << "SETLMDB: container '" << orderstate::orderName(db)
                  << "' TAG '" << orderstate::activeTag(db) << "' ("
                  << (orderstate::isAscending(db) ? "ASC" : "DESC") << ")\n";
        if (const auto* im = db.indexManagerPtr(); im && im->hasBackend()) {
            std::cout << "  backend: " << (im->isCdx() ? "CDX/LMDB" : "OTHER") << "\n";
        }
        return;
    }

    // Clear form
    if (tok == "0") {
        orderstate::clearOrder(db);
        db.indexManager().close();
        std::cout << "SETLMDB: cleared (physical order).\n";
        return;
    }

    // Parse optional tag + flags
    std::string chosenTag;
    std::string flag;
    iss >> chosenTag;
    iss >> flag;

    bool asc = true;
    if (!flag.empty()) {
        const std::string f = upper_copy(flag);
        if (f == "--DESC") asc = false;
        if (f == "--ASC")  asc = true;
    }

    const std::string container = resolve_container_path(tok);

    if (chosenTag.empty()) chosenTag = "LNAME";
    const std::string tag = upper_copy(chosenTag);

    // Persist per-area order state (legacy contract)
    orderstate::setOrder(db, container);
    orderstate::setActiveTag(db, tag);
    orderstate::setAscending(db, asc);

    // Open per-area backend now (no global LMDB state).
    {
        std::string err;
        if (!db.indexManager().openCdx(container, tag, &err)) {
            std::cout << "SETLMDB: error: " << (err.empty() ? "openCdx failed" : err) << "\n";
            // keep state consistent
            orderstate::clearOrder(db);
            db.indexManager().close();
            return;
        }
    }

    std::cout << "SETLMDB: using CDX '" << container << "' TAG '" << tag << "' ("
              << (asc ? "ASC" : "DESC") << ")\n";
    std::cout << "  envdir: " << (container + ".d") << "\n";
}

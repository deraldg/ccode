// src/cli/cmd_erase.cpp
// ERASE — physically deletes a table file and its same-stem sidecars.
//
// Supported syntax:
//   ERASE <table> [CONFIRM]
//   ERASE TABLE <table> [CONFIRM]
//
// Examples:
//   ERASE TABLE clients CONFIRM
//   ERASE students.dbf CONFIRM
//
// Behavior:
//   - Resolves <table> to a .dbf path (adds .dbf if missing).
//   - If not found as-given and path is relative, tries common roots: ./dbf, ./DBF, ./data/dbf, ./data/DBF
//   - Deletes the DBF plus known sidecars with the same stem in the same directory:
//       .fpt .dbt .inx .cnx .cdx .idx .dtx .dti.json .schema.json
//   - Safety gate: without CONFIRM, it prints what it *would* delete and does nothing.

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "command_registry.hpp"
#include "textio.hpp"
#include "xbase.hpp"

namespace fs = std::filesystem;

static inline std::string s8(const fs::path& p) {
#if defined(_WIN32)
    auto u = p.u8string();
    return std::string(u.begin(), u.end());
#else
    return p.string();
#endif
}

static bool ieq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::toupper(static_cast<unsigned char>(a[i])) !=
            std::toupper(static_cast<unsigned char>(b[i]))) return false;
    }
    return true;
}

static bool has_ext_ci(const std::string& s, const std::string& ext_with_dot) {
    if (s.size() < ext_with_dot.size()) return false;
    const size_t off = s.size() - ext_with_dot.size();
    return ieq(s.substr(off), ext_with_dot);
}

static fs::path normalize_to_dbf_path(std::string table_arg) {
    // If user passed "clients" -> "clients.dbf"
    // If they passed "clients.dbf" -> keep
    // If they passed other extension -> keep as-is (but we still *expect* a DBF to exist)
    if (!has_ext_ci(table_arg, ".dbf")) {
        // Only append .dbf if there is no extension at all (simple heuristic: no dot in final path element)
        fs::path p(table_arg);
        auto fn = p.filename().string();
        if (fn.find('.') == std::string::npos) {
            table_arg += ".dbf";
        }
    }
    return fs::path(table_arg);
}

static bool try_resolve_existing(const fs::path& in, fs::path& out) {
    std::error_code ec;
    if (fs::exists(in, ec)) { out = in; return true; }

    // If relative, try common roots used in this repo’s workflows.
    if (!in.is_absolute()) {
        const std::vector<fs::path> roots = {
            fs::path("."), fs::path("dbf"), fs::path("DBF"),
            fs::path("data") / "dbf",
            fs::path("data") / "DBF",
        };

        for (const auto& r : roots) {
            fs::path cand = r / in;
            if (fs::exists(cand, ec)) { out = cand; return true; }
        }
    }

    return false;
}

static std::vector<fs::path> build_sidecar_list(const fs::path& dbf_path) {
    // Same directory, same stem
    const fs::path dir  = dbf_path.parent_path();
    const std::string stem = dbf_path.stem().string(); // "clients" from "clients.dbf"

    std::vector<fs::path> files;
    files.reserve(12);

    // Primary
    files.push_back(dbf_path);

    // Traditional DBF sidecars (optional)
    files.push_back(dir / (stem + ".fpt"));
    files.push_back(dir / (stem + ".dbt"));

    // Index containers / common index files (optional)
    files.push_back(dir / (stem + ".inx"));
    files.push_back(dir / (stem + ".cnx"));
    files.push_back(dir / (stem + ".cdx"));
    files.push_back(dir / (stem + ".idx"));

    // DotTalk++ sidecars (optional)
    files.push_back(dir / (stem + ".dtx"));         // memo sidecar
    files.push_back(dir / (stem + ".dti.json"));    // indexing stub sidecar
    files.push_back(dir / (stem + ".schema.json")); // schema sidecar

    // Dedup
    std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b){
        return s8(a) < s8(b);
    });
    files.erase(std::unique(files.begin(), files.end(), [](const fs::path& a, const fs::path& b){
        return ieq(s8(a), s8(b));
    }), files.end());

    return files;
}

static void print_usage() {
    std::cout
        << "Usage:\n"
        << "  ERASE <table> [CONFIRM]\n"
        << "  ERASE TABLE <table> [CONFIRM]\n"
        << "Notes:\n"
        << "  - Physically deletes <table>.dbf and known same-stem sidecars.\n"
        << "  - Without CONFIRM, performs a dry-run (prints what would be deleted).\n";
}

void cmd_ERASE(xbase::DbArea& /*area*/, std::istringstream& iss) {
    std::string tok;
    if (!(iss >> tok)) { print_usage(); return; }

    std::string table_arg;
    bool confirm = false;

    // Accept optional "TABLE"
    if (textio::up(tok) == "TABLE") {
        if (!(iss >> table_arg) || table_arg.empty()) { print_usage(); return; }
    } else {
        table_arg = tok;
    }

    // Optional trailing CONFIRM
    std::string tail;
    while (iss >> tail) {
        if (textio::up(tail) == "CONFIRM" || tail == "/Y" || tail == "-Y") {
            confirm = true;
        }
        // ignore other tokens for now
    }

    fs::path wanted = normalize_to_dbf_path(table_arg);

    fs::path dbf_path;
    if (!try_resolve_existing(wanted, dbf_path)) {
        std::cout << "ERASE: Table not found: " << s8(wanted) << "\n";
        return;
    }

    auto files = build_sidecar_list(dbf_path);

    // Filter to ones that actually exist
    std::error_code ec;
    std::vector<fs::path> existing;
    existing.reserve(files.size());
    for (const auto& f : files) {
        if (fs::exists(f, ec)) existing.push_back(f);
    }

    if (existing.empty()) {
        std::cout << "ERASE: Nothing to delete for: " << s8(dbf_path) << "\n";
        return;
    }

    // Dry-run unless confirmed
    if (!confirm) {
        std::cout << "ERASE (dry-run): would delete " << existing.size()
                  << " file(s) for table: " << s8(dbf_path.stem()) << "\n";
        for (const auto& f : existing) std::cout << "  " << s8(f.filename()) << "\n";
        std::cout << "Re-run with CONFIRM to perform deletion.\n";
        return;
    }

    int deleted = 0;
    int failed  = 0;

    std::cout << "ERASE: deleting " << existing.size()
              << " file(s) for table: " << s8(dbf_path.stem()) << "\n";

    for (const auto& f : existing) {
        ec.clear();
        fs::remove(f, ec);
        if (ec) {
            ++failed;
            std::cout << "  FAILED: " << s8(f.filename()) << "  (" << ec.message() << ")\n";
        } else {
            ++deleted;
            std::cout << "  Deleted: " << s8(f.filename()) << "\n";
        }
    }

    std::cout << "ERASE complete. Deleted: " << deleted << ", Failed: " << failed << "\n";
}

static bool s_registered = []() {
    dli::registry().add("ERASE", &cmd_ERASE);
    return true;
}();

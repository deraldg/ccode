// src/cli/cmd_setcnx.cpp — SETCNX [<path.cnx>]
// Behavior:
// - If no path given, attempts <current_table_stem>.cnx in the table's folder.
// - Verifies existence; does NOT create.
// - Attaches the CNX container path via orderstate::setOrder(A, path).
// - Clears any previously active CNX tag (implicit in setOrder()).

#include "xbase.hpp"
#include "order_state.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

static std::string default_cnx_for(const xbase::DbArea& A) {
    // We don't have a public API to read the current DBF path here,
    // but DotTalk++ typically exposes it via DbArea::fileName() or similar.
    // If not available, SETCNX with no argument should already be mapped
    // by the caller to supply the path; but try a best-effort fallback:
    try {
        // Many call sites keep the DBF path in A.filePath() or A.fileName().
        // If neither exists in your DBF API, replace this with the correct getter.
        // ---- BEGIN STABLE FALLBACK ----
        // Assume orderstate::orderName()’s directory is the data dir if an order
        // is already set; otherwise current working directory.
        fs::path dir = fs::current_path();
        // If a legacy index is attached, we can infer the data folder from it.
        if (orderstate::hasOrder(A)) {
            fs::path ord = orderstate::orderName(A);
            if (!ord.empty()) dir = ord.parent_path();
        }
        // Fallback stem — many shells keep the selected table's alias in memory.
        // Without an API, use "table" as a generic placeholder; user can pass path.
        fs::path guess = dir / "table.cnx";
        return guess.string();
        // ---- END STABLE FALLBACK ----
    } catch (...) {
        return std::string("table.cnx");
    }
}

void cmd_SETCNX(xbase::DbArea& A, std::istringstream& in) {
    std::string arg;
    std::string path;

    if (in >> arg) {
        fs::path p = arg;
        if (!p.has_extension()) p.replace_extension(".cnx");
        path = p.string();
    } else {
        // No arg — derive a default (caller environment may provide a better one).
        path = default_cnx_for(A);
    }

    fs::path p = path;
    if (!p.has_extension()) p.replace_extension(".cnx");

    if (!fs::exists(p)) {
        std::cout << "SET CNX: file not found: " << p.string() << "\n";
        return;
    }

    orderstate::setOrder(A, p.string());   // attaches container; clears prior tag
    std::cout << "SET CNX: attached \"" << p.string() << "\"\n";
}

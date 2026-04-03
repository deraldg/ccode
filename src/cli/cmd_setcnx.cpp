// src/cli/cmd_setcnx.cpp
// SET CNX [<name-or-path>]
// Resolve CNX using SET PATH INDEXES slot via cli/path_resolver.

#include "xbase.hpp"
#include "cli/order_state.hpp"
#include "cli/path_resolver.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

static fs::path resolve_cnx_token(const std::string& tok)
{
    // Reuse existing INDEX slot resolver.
    // If no extension was provided, default to .cnx.
    fs::path p = dottalk::paths::resolve_index(tok);
    if (!p.has_extension()) p.replace_extension(".cnx");
    return p;
}

static fs::path default_cnx_for(const xbase::DbArea& A)
{
    // Area-aware default: if a DBF is open, use its basename; otherwise fallback.
    std::string stem;

    // If your DbArea lacks these accessors, replace with what you actually have.
    // Common choices in your tree: logicalName(), name(), dbfBasename(), etc.
    if (A.isOpen()) {
        // name() in your tree prints the open filename/path-ish identifier.
        // Convert to stem safely.
        fs::path n(A.name());
        stem = n.stem().string();
        if (stem.empty()) stem = "table";
    } else {
        stem = "table";
    }

    return resolve_cnx_token(stem);
}

void cmd_SETCNX(xbase::DbArea& A, std::istringstream& in)
{
    std::string arg;
    fs::path p;

    if (in >> arg) p = resolve_cnx_token(arg);
    else          p = default_cnx_for(A);

    if (!fs::exists(p)) {
        std::cout << "SET CNX: file not found: " << p.string() << "\n";
        return;
    }

    try {
        // CNX attachment/state is owned by order subsystem in your architecture.
        orderstate::setOrder(A, p.string());
        std::cout << "SET CNX: attached \"" << p.string() << "\"\n";
    }
    catch (const std::exception& ex) {
        std::cout << "SET CNX failed: " << ex.what() << "\n";
    }
}

// src/cli/cmd_setcdx.cpp
// SET CDX [<name-or-path>]
// Resolve CDX using SET PATH INDEXES slot via cli/path_resolver.

#include "xbase.hpp"
#include "cli/order_state.hpp"
#include "cli/path_resolver.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

static fs::path resolve_cdx_token(const std::string& tok)
{
    // Reuse existing INDEX slot resolver.
    // If no extension was provided, default to .cdx.
    fs::path p = dottalk::paths::resolve_index(tok);
    if (!p.has_extension()) p.replace_extension(".cdx");
    return p;
}

static fs::path default_cdx_for(const xbase::DbArea& A)
{
    // Area-aware default: if a DBF is open, use its basename; otherwise fallback.
    std::string stem;

    if (A.isOpen()) {
        fs::path n(A.name());
        stem = n.stem().string();
        if (stem.empty()) stem = "table";
    } else {
        stem = "table";
    }

    return resolve_cdx_token(stem);
}

void cmd_SETCDX(xbase::DbArea& A, std::istringstream& in)
{
    std::string arg;
    fs::path p;

    if (in >> arg) p = resolve_cdx_token(arg);
    else          p = default_cdx_for(A);

    if (!fs::exists(p)) {
        std::cout << "SET CDX: file not found: " << p.string() << "\n";
        return;
    }

    try {
        // CDX attachment/state is owned by order subsystem in your architecture.
        orderstate::setOrder(A, p.string());
        std::cout << "SET CDX: attached \"" << p.string() << "\"\n";
    }
    catch (const std::exception& ex) {
        std::cout << "SET CDX failed: " << ex.what() << "\n";
    }
}
// src/cli/cmd_status.cpp
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cctype>

#include "xbase.hpp"
#include "order_state.hpp"
// Prefer in-memory tag list + expressions if available
#include "xindex/attach.hpp"
#include "xindex/index_manager.hpp"

namespace fs = std::filesystem;

static std::string upper(std::string s) {
    for (auto &ch : s) ch = (char)std::toupper((unsigned char)ch);
    return s;
}

void cmd_STATUS(xbase::DbArea& a, std::istringstream& iss) {
    (void)iss;
    if (!a.isOpen()) {
        std::cout << "No file open\n";
        return;
    }

    // --- Workspace ---
    std::cout << "Workspace\n";
    std::cout << "  File: " << a.name() << "\n\n";

    // --- Table ---
    std::cout << "Table\n";
    std::cout << "  Records: " << a.recCount() << "\n";
    std::cout << "  Recno:   " << a.recno() << "\n";
    std::cout << "  Bytes/rec: " << a.cpr() << "\n\n";

    // --- Order / Index ---
    std::cout << "Order / Index\n";
    const bool asc = orderstate::isAscending(a);
    std::cout << "  Order direction: " << (asc ? "ASCEND" : "DESCEND") << "\n";

    // Active tag: derive from orderstate name (which may be a full path)
    std::string activeTag = "(none)";
    fs::path scanDir; // we'll also derive a stable directory to scan
    {
        std::string name = orderstate::orderName(a);     // may be full path to .inx
        if (!name.empty()) {
            fs::path p(name);
            scanDir = p.parent_path();
            std::string stem = p.stem().string();
            if (!stem.empty()) activeTag = upper(stem);
        }
        if (scanDir.empty()) scanDir = fs::path(a.name()).parent_path();
        if (scanDir.empty()) scanDir = fs::current_path();
    }
    std::cout << "  Active tag: " << activeTag << "\n";

    // Prefer in-memory manager (has expressions), else fall back to disk scan
    std::vector<std::string> tags;
    bool printed = false;
    try {
        auto& mgr = xindex::ensure_manager(a);
        tags = mgr.listTags();
        if (!tags.empty()) {
            std::cout << "  Index tags loaded: " << tags.size() << "\n";
            for (auto const& t : tags) {
                const bool isActive = (upper(t) == upper(activeTag));
                std::cout << (isActive ? "    * " : "      ")
                          << t << "  -> " << mgr.exprFor(t) << "\n";
            }
            printed = true;
        }
    } catch (...) { /* manager not attached yet */ }

    if (!printed) {
        // Fallback: scan .inx files in the stable folder
        try {
            for (auto const& de : fs::directory_iterator(scanDir)) {
                if (de.is_regular_file() && de.path().extension() == ".inx") {
                    tags.push_back(upper(de.path().stem().string()));
                }
            }
        } catch (...) { /* ignore */ }

        std::cout << "  Index tags loaded: " << tags.size() << "\n";
        for (const auto& t : tags) {
            bool isActive = (upper(t) == upper(activeTag));
            std::cout << (isActive ? "    * " : "      ") << t << "\n";
        }
    }
}

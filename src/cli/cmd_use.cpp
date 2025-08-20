// src/cli/cmd_use.cpp
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>
#include "xbase.hpp"
#include "textio.hpp"
#include "order_state.hpp"

using namespace xbase;
namespace fs = std::filesystem;

static std::string read_rest_upper_trim(std::istringstream& iss) {
    std::string rest;
    std::getline(iss >> std::ws, rest);
    return textio::up(textio::trim(rest));
}

void cmd_USE(DbArea& a, std::istringstream& iss) {
    // ---- parse required db name ----
    std::string db;
    if (!(iss >> db)) {
        std::cout << "Usage: USE <dbf> [NOINDEX]\n";
        return;
    }

    // ---- parse optional tail (e.g., NOINDEX) ----
    const std::string rest = read_rest_upper_trim(iss);
    bool bypassIndex = (rest.find("NOINDEX") != std::string::npos);

    // ---- normalize path (.dbf default) ----
    fs::path path = db;
    if (path.extension().empty()) {
        path.replace_extension(".dbf");
    }

    if (!fs::exists(path)) {
        std::cout << "Open failed: file not found\n";
        return;
    }

    // ---- close any existing order for this area ----
    orderstate::clearOrder(a);

    // ---- open table ----
    try {
        a.open(path.string());
        std::cout << "Opened " << path.filename().string()
                  << " with " << a.recCount() << " records.\n";
    } catch (const std::exception& e) {
        std::cout << "Open failed: " << e.what() << "\n";
        return;
    }

    // ---- auto-attach <opened>.inx unless NOINDEX ----
    if (!bypassIndex) {
        fs::path idx = a.name(); // actual opened path from DbArea
        idx.replace_extension(".inx");
        try {
            if (fs::exists(idx)) {
                orderstate::setOrder(a, idx.string());
                // Optional: std::cout << "Order set to: " << orderstate::orderName(a) << "\n";
            }
        } catch (...) {
            // ignore filesystem/index attach errors; keep table open
        }
    }
}

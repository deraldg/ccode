#include "xbase.hpp"
#include "textio.hpp"
#include "order_state.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>

using namespace xbase;
namespace fs = std::filesystem;

static std::string up(std::string s) { return textio::up(s); }

void cmd_SETINDEX(DbArea& a, std::istringstream& iss) {
    if (!a.isOpen()) { std::cout << "No table open.\n"; return; }

    std::string tag;
    if (!(iss >> tag)) {
        std::cout << "Usage: SET INDEX <tag>\n";
        return;
    }

    // Prefer <table>.inx; fallback to <tag>.inx next to table
    std::error_code ec;
    fs::path idx = a.name();
    idx.replace_extension(".inx");

    if (!fs::exists(idx, ec)) {
        fs::path alt = fs::path(a.name()).replace_filename(tag + ".inx");
        if (fs::exists(alt, ec)) {
            idx = alt;
        } else {
            std::cout << "No index file: "
                      << idx.filename().string() << " or "
                      << alt.filename().string() << "\n";
            return;
        }
    }

    try {
        // If your orderstate has a tag-aware overload, you can pass up(tag) too.
        // Using the path-only overload here for broad compatibility.
        orderstate::setOrder(a, idx.string());

	

        std::cout << "Index set: " << idx.filename().string()
                  << " (tag requested: " << up(tag) << ")\n";
    } catch (const std::exception& e) {
        std::cout << "SET INDEX failed: " << e.what() << "\n";
    }
}

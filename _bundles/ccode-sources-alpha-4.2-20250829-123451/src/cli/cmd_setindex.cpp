#include "xbase.hpp"        // <-- Needed for DbArea
#include "textio.hpp"
#include "order_state.hpp"
#include <filesystem>
#include <iostream>
#include <sstream>

using namespace textio;
namespace fs = std::filesystem;

// SETINDEX <tag|path>
void cmd_SETINDEX(xbase::DbArea& A, std::istringstream& in) {
    std::string target;
    if (!(in >> target)) { std::cout << "Usage: SETINDEX <tag>\n"; return; }

/*
void cmd_SETINDEX(DbArea& A, std::istringstream& in)
{
    std::string target;
    if (!(in >> target)) {
        std::cout << "Usage: SETINDEX <tag|path>\n";
        return;
    }
*/
    // Resolve <target> into a concrete .inx file path.
    fs::path p = target;
    if (!p.has_extension()) {
        p.replace_extension(".inx");
    }

    // If no directory component was provided, assume current working directory.
    if (p.parent_path().empty()) {
        if (!fs::exists(p)) {
            std::cout << "Index file not found: " << p.string() << "\n";
            return;
        }
    } else {
        if (!fs::exists(p)) {
            std::cout << "Index file not found: " << p.string() << "\n";
            return;
        }
    }

    // Activate the index for this area.
    orderstate::setOrder(A, p.string());
    std::cout << "Index set: " << p.string() << "\n";
}

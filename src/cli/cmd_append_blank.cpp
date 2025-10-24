// src/cli/cmd_append_blank.cpp
#include <sstream>
#include <string>

namespace xbase { class DbArea; }

// Implemented in cmd_append.cpp
void cmd_APPEND(xbase::DbArea& A, std::istringstream& S);

// Helper: get remaining unread text from an istringstream
static std::string remaining(std::istringstream& in) {
    const std::string& all = in.str();
    const std::streampos p = in.tellg();
    if (p == std::streampos(-1)) return all;                    // tellg() not set -> use full buffer
    const size_t i = static_cast<size_t>(p);
    return i < all.size() ? all.substr(i) : std::string{};
}

// dBASE-style "APPEND BLANK" wrapper:
// Calls the normal APPEND handler but ensures "BLANK" is seen as the first argument.
void cmd_APPEND_BLANK(xbase::DbArea& A, std::istringstream& S) {
    std::istringstream proxy("BLANK " + remaining(S));
    cmd_APPEND(A, proxy);
}

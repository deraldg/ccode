#include "cli/script_reader.hpp"

#include <cctype>
#include <string>

namespace {

static bool leading_command_is_sqlite(const std::string& s)
{
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    if (i >= s.size()) return false;

    if (s[i] == '.') {
        std::size_t j = i + 1;
        if (j < s.size() && std::isspace(static_cast<unsigned char>(s[j]))) {
            i = j;
            while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
                ++i;
            }
            if (i >= s.size()) return false;
        }
    }

    std::size_t j = i;
    while (j < s.size() && !std::isspace(static_cast<unsigned char>(s[j]))) {
        ++j;
    }

    if (j <= i) return false;

    std::string first = s.substr(i, j - i);
    for (char& ch : first) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }

    return first == "SQLITE";
}

static std::string strip_hash_comment(const std::string& s)
{
    bool in_single = false;
    bool in_double = false;
    bool esc = false;

    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];

        if (esc) {
            esc = false;
            continue;
        }
        if (c == '\\') {
            esc = true;
            continue;
        }
        if (!in_double && c == '\'') {
            in_single = !in_single;
            continue;
        }
        if (!in_single && c == '"') {
            in_double = !in_double;
            continue;
        }

        if (!in_single && !in_double && c == '#') {
            std::size_t j = i;
            while (j > 0 && (s[j - 1] == ' ' || s[j - 1] == '\t')) {
                --j;
            }
            return s.substr(0, j);
        }
    }

    return s;
}

static bool last_semicolon_is_outside_quotes(const std::string& s)
{
    if (leading_command_is_sqlite(s)) return false;

    bool in_single = false;
    bool in_double = false;
    bool esc = false;

    for (char c : s) {
        if (esc) {
            esc = false;
            continue;
        }
        if (c == '\\') {
            esc = true;
            continue;
        }
        if (!in_double && c == '\'') {
            in_single = !in_single;
            continue;
        }
        if (!in_single && c == '"') {
            in_double = !in_double;
            continue;
        }
    }

    if (in_single || in_double) return false;

    std::size_t i = s.find_last_not_of(" \t\r");
    return i != std::string::npos && s[i] == ';';
}

static void trim_trailing_cr(std::string& s)
{
    if (!s.empty() && s.back() == '\r') {
        s.pop_back();
    }
}

} // namespace

bool read_script_command(std::istream& in, std::string& out)
{
    out.clear();

    std::string line;
    if (!std::getline(in, line)) {
        return false;
    }

    trim_trailing_cr(line);
    line = strip_hash_comment(line);

    std::string accum = line;

    while (last_semicolon_is_outside_quotes(accum)) {
        while (!accum.empty() &&
               (accum.back() == ' ' ||
                accum.back() == '\t' ||
                accum.back() == '\r' ||
                accum.back() == ';')) {
            const char c = accum.back();
            accum.pop_back();
            if (c == ';') break;
        }

        std::string more;
        if (!std::getline(in, more)) {
            break;
        }

        trim_trailing_cr(more);
        more = strip_hash_comment(more);

        if (!accum.empty() && !more.empty()) {
            accum.push_back(' ');
        }
        accum += more;
    }

    out = accum;
    return true;
}
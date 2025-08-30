// helpers (e.g., src/cli/parse_utils.hpp/.cpp)
#include <string>
#include <cctype>
#include <algorithm>

static std::string uptrim(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c){return !std::isspace(c);} ));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c){return !std::isspace(c);} ).base(), s.end());
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

static bool parse_logical(std::string s, bool& out) {
    s = uptrim(s);
    if (s == ".T." || s == "T" || s == "Y" || s == "1" || s == "TRUE")  { out = true;  return true; }
    if (s == ".F." || s == "F" || s == "N" || s == "0" || s == "FALSE") { out = false; return true; }
    return false;
}

static bool digits_only(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}

// Accept YYYYMMDD, YYYY-MM-DD, MM/DD/YYYY
static bool parse_date(std::string s, std::string& yyyymmdd) {
    s = uptrim(s);
    if (digits_only(s) && s.size() == 8) { yyyymmdd = s; return true; }
    if (s.size() == 10 && s[4]=='-' && s[7]=='-') { // YYYY-MM-DD
        yyyymmdd = s.substr(0,4)+s.substr(5,2)+s.substr(8,2); return digits_only(yyyymmdd);
    }
    if (s.size() == 10 && s[2]=='/' && s[5]=='/') { // MM/DD/YYYY
        yyyymmdd = s.substr(6,4)+s.substr(0,2)+s.substr(3,2); return digits_only(yyyymmdd);
    }
    return false;
}

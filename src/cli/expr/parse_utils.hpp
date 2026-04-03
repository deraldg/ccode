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


// === Upgrades: general-purpose parsing helpers ==============================
// Detect arithmetic operators + - * / or parentheses outside of quotes.
// Useful to decide when to bypass "simple" evaluators and use the compiler.
static inline bool has_unquoted_arith(const std::string& s) {
    bool in_single = false, in_double = false;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"' && !in_single) { in_double = !in_double; continue; }
        if (c == '\'' && !in_double) { in_single = !in_single; continue; }
        if (in_single || in_double) continue;
        if (c=='+' || c=='-' || c=='*' || c=='/' || c=='(' || c==')')
            return true;
    }
    return false;
}

// Strip end-of-line comments (// and FoxPro-style &&) that are OUTSIDE quotes.
// Preserves text within single/double quotes and trims surrounding whitespace.
static inline std::string strip_line_comments(const std::string& in){
    std::string out; out.reserve(in.size());
    bool in_s=false, in_d=false;
    for (size_t i=0; i<in.size(); ++i){
        char c=in[i];
        char n=(i+1<in.size()? in[i+1] : '\0');
        if(!in_s && !in_d){
            if(c=='/' && n=='/'){ break; }   // C++-style // comment
            if(c=='&' && n=='&'){ break; }   // FoxPro-style && comment
        }
        if(c=='"' && !in_s){ in_d=!in_d; out.push_back(c); continue; }
        if(c=='\'' && !in_d){ in_s=!in_s; out.push_back(c); continue; }
        out.push_back(c);
    }
    // trim trailing/leading spaces
    while(!out.empty() && std::isspace((unsigned char)out.back())) out.pop_back();
    size_t j=0; while(j<out.size() && std::isspace((unsigned char)out[j])) ++j;
    return out.substr(j);
}

// Simple unsigned-integer check (no sign). Handy for command parsers.
static inline bool is_uint(const std::string& s) {
    if (s.empty()) return false;
    for (unsigned char c : s) if (!std::isdigit(c)) return false;
    return true;
}




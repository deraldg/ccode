#include "browse_util.hpp"
#include <cctype>
#include <climits>
#include <cstdlib>
#include <sstream>

namespace dottalk::browse::util {

std::string up(std::string s){
    for (auto& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

bool ieq(const std::string& a, const std::string& b){
    return up(a) == up(b);
}

bool parse_int(const std::string& s, int& out){
    if (s.empty()) return false;
    char* e = nullptr;
    long v = std::strtol(s.c_str(), &e, 10);
    if (e == s.c_str() || *e != '\0') return false;
    if (v < INT_MIN || v > INT_MAX) return false;
    out = static_cast<int>(v);
    return true;
}

std::vector<std::string> tokenize(std::istringstream& in){
    std::vector<std::string> out; std::string tok;
    while (in >> tok) out.push_back(tok);
    return out;
}

std::string extract_for_expr(const std::vector<std::string>& t){
    int n = (int)t.size(), i = -1;
    for (int k = 0; k < n; ++k) if (ieq(t[k], "FOR")) { i = k; break; }
    if (i < 0 || i + 1 >= n) return "";
    auto stop = [](const std::string& s){
        return s == "RAW" || s == "PRETTY" || s == "PAGE" || s == "ALL" ||
               s == "TOP" || s == "BOTTOM" || s == "START" || s == "QUIET" ||
               s == "EDIT" || s == "SESSION";
    };
    std::ostringstream o;
    for (int k = i + 1; k < n; ++k){
        std::string upk = up(t[k]);
        if (stop(upk)) break;
        if (k > i + 1) o << ' ';
        o << t[k];
    }
    return o.str();
}

static std::string extract_start_key_tokens(const std::vector<std::string>& t, size_t from){
    std::ostringstream o;
    for (size_t i = from; i < t.size(); ++i){
        if (i > from) o << ' ';
        o << t[i];
    }
    return o.str();
}

std::string extract_start_key(const std::vector<std::string>& t){
    for (size_t i = 0; i + 2 < t.size(); ++i){
        if (ieq(t[i], "START") && ieq(t[i + 1], "KEY")){
            const std::string& k = t[i + 2];
            if (!k.empty() && (k.front()=='\'' || k.front()=='\"')){
                return extract_start_key_tokens(t, i + 2);
            }
            return k;
        }
    }
    return "";
}

} // namespace dottalk::browse::util

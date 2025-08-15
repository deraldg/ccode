#include "csv.hpp"

namespace csv {

std::vector<std::string> split_line(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    bool in_quotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (in_quotes) {
            if (c == '\"') {
                if (i + 1 < line.size() && line[i+1] == '\"') {
                    cur.push_back('\"'); ++i;
                } else {
                    in_quotes = false;
                }
            } else cur.push_back(c);
        } else {
            if (c == ',') { out.push_back(cur); cur.clear(); }
            else if (c == '\"') in_quotes = true;
            else cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

std::string escape(const std::string& s) {
    bool need_quote = s.find_first_of(",\"\n\r") != std::string::npos;
    if (!need_quote) return s;
    std::string t = "\"";
    for (char c : s) { if (c == '\"') t.push_back('\"'); t.push_back(c); }
    t.push_back('\"');
    return t;
}

} // namespace csv

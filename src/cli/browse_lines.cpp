// src/cli/browse_lines.cpp
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>
#include "cli/browsetui.hpp"

// Helper to trim trailing spaces/tabs etc.
static inline void rtrim_inplace(std::string& s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.pop_back();
}

static inline std::string pad_right(const std::string& s, int n) {
    if (n <= 0) return std::string();
    if (static_cast<int>(s.size()) >= n) return s.substr(0, static_cast<size_t>(n));
    std::string t = s;
    t.append(static_cast<size_t>(n - static_cast<int>(s.size())), ' ');
    return t;
}

// Definition to satisfy the call sites in cmd_browsetui.cpp
std::vector<std::string>
build_browse_lines(int inner_w, int inner_h, int recno,
                   const std::vector<FieldView>& fields)
{
    std::vector<std::string> out;
    if (inner_w <= 0 || inner_h <= 0) return out;
    out.reserve(static_cast<size_t>(inner_h));

    // Header line: " REC <n>"
    if (inner_h > 0) {
        std::string head = "REC " + std::to_string(recno);
        if (static_cast<int>(head.size()) > inner_w) head.resize(static_cast<size_t>(inner_w));
        out.push_back(" " + std::move(head));
    }

    // Figure label width: cap at 1/3 of the inner width, max 32, min 6
    int max_name = 0;
    for (const auto& fv : fields)
        max_name = std::max(max_name, static_cast<int>(fv.name.size()));
    const int label_w = std::max(6, std::min({ max_name, inner_w / 3, 32 }));

    // Lines: " <name-padded> <value>"
    for (size_t i = 0; i < fields.size() && static_cast<int>(out.size()) < inner_h; ++i) {
        std::string name = fields[i].name;
        std::string val  = fields[i].value;
        rtrim_inplace(val);

        std::string line;
        line.reserve(static_cast<size_t>(inner_w));
        line.push_back(' ');
        line += pad_right(name, label_w);
        if (static_cast<int>(line.size()) < inner_w) line.push_back(' ');

        int avail = inner_w - static_cast<int>(line.size());
        if (avail > 0) {
            if (static_cast<int>(val.size()) > avail) val.resize(static_cast<size_t>(avail));
            line += val;
        }
        if (static_cast<int>(line.size()) < inner_w)
            line.append(static_cast<size_t>(inner_w - static_cast<int>(line.size())), ' ');

        out.push_back(std::move(line));
    }

    // Pad out remaining lines with spaces
    while (static_cast<int>(out.size()) < inner_h)
        out.emplace_back(static_cast<size_t>(inner_w), ' ');

    return out;
}

#include "filter_registry.hpp"

#include <cctype>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "xbase.hpp"

// real API
#include "cli/expr/api.hpp"
#include "cli/expr/ast.hpp"

// field lookup helpers
namespace filter_detail {

inline int find_field_ci(const std::vector<xbase::FieldDef>& fields,
                         const std::string& name)
{
    auto up = [](char c) { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); };

    for (size_t i = 0; i < fields.size(); ++i) {
        const auto& f = fields[i].name;
        if (f.size() != name.size()) continue;

        bool match = true;
        for (size_t j = 0; j < f.size(); ++j) {
            if (up(f[j]) != up(name[j])) {
                match = false;
                break;
            }
        }
        if (match) return static_cast<int>(i);
    }
    return -1;
}

inline std::string normalize_char(std::string s)
{
    // Trim right padding (classic DBF CHAR behavior).
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }

    // Trim left defensively.
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
        ++i;
    }
    if (i > 0) {
        s.erase(0, i);
    }

    // Uppercase for case-insensitive comparisons.
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    return s;
}

inline std::string get_field_str(xbase::DbArea& area,
                                 const std::string& name)
{
    int idx = find_field_ci(area.fields(), name);
    if (idx < 0) return {};

    std::string s = area.get(idx + 1);
    return normalize_char(s);
}

inline std::optional<double> get_field_num(xbase::DbArea& area,
                                           const std::string& name)
{
    int idx = find_field_ci(area.fields(), name);
    if (idx < 0) return std::nullopt;

    std::string s = area.get(idx + 1);
    try {
        return std::stod(s);
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace filter_detail

#define DOTTALK_GET_FIELD_STR(area, name) filter_detail::get_field_str(area, name)
#define DOTTALK_GET_FIELD_NUM(area, name) filter_detail::get_field_num(area, name)

#include "cli/expr/glue_xbase.hpp"

using dottalk::expr::Expr;

namespace {
struct State {
    std::shared_ptr<Expr> ast;
    std::string text;
};

std::unordered_map<xbase::DbArea*, State> g_map;
std::mutex g_mx;
} // namespace

namespace filter {

bool set(xbase::DbArea* area, const std::string& text, std::string& err)
{
    if (!area) return false;

    auto cr = dottalk::expr::compile_where(text);
    if (!cr) {
        err = cr.error;
        return false;
    }

    std::lock_guard<std::mutex> lk(g_mx);
    g_map[area] = { std::shared_ptr<Expr>(std::move(cr.program)), text };
    return true;
}

void clear(xbase::DbArea* area)
{
    if (!area) return;

    std::lock_guard<std::mutex> lk(g_mx);
    g_map.erase(area);
}

bool has(xbase::DbArea* area)
{
    std::lock_guard<std::mutex> lk(g_mx);
    auto it = g_map.find(area);
    return it != g_map.end() && static_cast<bool>(it->second.ast);
}

bool has_active_filter(xbase::DbArea* area)
{
    if (!area) return false;

    std::lock_guard<std::mutex> lk(g_mx);
    auto it = g_map.find(area);
    if (it == g_map.end()) return false;

    return static_cast<bool>(it->second.ast) || !it->second.text.empty();
}

bool visible(xbase::DbArea* area,
             const std::shared_ptr<Expr>& for_ast)
{
    if (!area) return false;

    std::shared_ptr<Expr> fil_ast;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        auto it = g_map.find(area);
        if (it != g_map.end())
            fil_ast = it->second.ast;
    }

    auto rv = dottalk::expr::glue::make_record_view(*area);

    if (fil_ast && !fil_ast->eval(rv)) return false;
    if (for_ast && !for_ast->eval(rv)) return false;
    return true;
}

} // namespace filter
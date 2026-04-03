#include "browse_args.hpp"
#include "browse_util.hpp"
#include <string>

namespace dottalk::browse {

static bool looks_like_expression(const std::vector<std::string>& toks){
    // Light heuristic: any comparison/operator token → it's an expression
    for (auto& t : toks){
        auto u = dottalk::browse::util::up(t);
        if (u == "=" || u == "==" || u == "!=" || u == "<>" ||
            u == ">" || u == "<" || u == ">=" || u == "<=" ||
            u == "LIKE" || u == "CONTAINS" || u == "BEGINS" || u == "ENDS"){
            return true;
        }
    }
    return false;
}

static std::string join_tokens(const std::vector<std::string>& toks){
    std::string s;
    for (size_t i=0;i<toks.size();++i){
        if (i) s.push_back(' ');
        s += toks[i];
    }
    return s;
}

BrowseArgs parse_args(std::istringstream& in){
    using namespace dottalk::browse::util;
    BrowseArgs a;
    auto toks = tokenize(in);

    // Extract explicit FOR and START KEY first (do not remove tokens—just detect).
    a.for_expr = extract_for_expr(toks);
    a.start_key_literal = extract_start_key(toks);

    // Options
    for (size_t i = 0; i < toks.size(); ++i){
        auto t = up(toks[i]);
        if (t == "RAW"){ a.want_raw = true; continue; }
        if (t == "PRETTY"){ a.want_raw = false; continue; }
        if (t == "ALL"){ a.list_all = true; continue; }
        if (t == "TOP"){ a.start = BrowseArgs::StartPos::Top; continue; }
        if (t == "BOTTOM"){ a.start = BrowseArgs::StartPos::Bottom; continue; }
        if (t == "QUIET"){ a.quiet = true; continue; }
        if ((t == "PAGE") && i + 1 < toks.size()){
            int n = 0; if (parse_int(toks[i+1], n) && n > 0){ a.page_size = n; ++i; }
            continue;
        }
        if (t == "EDIT" || t == "SESSION"){ a.interactive = true; continue; }
    }

    // If no explicit FOR, but the whole thing looks like an expression, treat it as implicit FOR.
    if (a.for_expr.empty() && looks_like_expression(toks)){
        a.for_expr = join_tokens(toks);
    }

    return a;
}

} // namespace dottalk::browse

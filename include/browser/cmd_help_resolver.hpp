#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <unordered_set>

#include "foxref.hpp"
#include "dotref.hpp"

namespace helpresolver
{

enum class Scope
{
    Auto,
    Fox,
    Dot
};

struct HelpMatch
{
    std::string name;
    std::string catalog;   // "fox" or "dot"
    std::string summary;
    bool alias_match = false;
    bool exact = false;
    int score = 0;
};

struct HelpResult
{
    bool exact = false;
    bool ambiguous = false;
    bool used_scope_filter = false;
    std::vector<HelpMatch> matches;
};

inline std::string upper(const std::string& s)
{
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return r;
}

inline std::string trim(const std::string& s)
{
    size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;

    size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;

    return s.substr(a, b - a);
}

inline bool starts_with_icase(const std::string& text, const std::string& key)
{
    if (key.size() > text.size()) return false;
    return upper(text.substr(0, key.size())) == upper(key);
}

inline bool contains_icase(const std::string& text, const std::string& key)
{
    return upper(text).find(upper(key)) != std::string::npos;
}

// Optional alias hooks.
// If your ref headers do not expose alias catalogs yet, these default stubs keep the resolver buildable.
struct AliasEntry
{
    const char* alias;
    const char* target;
};

namespace detail
{
    template <typename T>
    using CatalogFn = std::vector<T>(*)();

    inline std::vector<AliasEntry> empty_alias_catalog()
    {
        return {};
    }
}

// Provide these in foxref/dotref later if desired:
//   std::vector<AliasEntry> alias_catalog();
// Until then, the resolver will simply skip alias expansion.

inline std::vector<AliasEntry> fox_alias_catalog_fallback()
{
#ifdef HELPRESOLVER_USE_FOX_ALIAS_CATALOG
    return foxref::alias_catalog();
#else
    return {};
#endif
}

inline std::vector<AliasEntry> dot_alias_catalog_fallback()
{
#ifdef HELPRESOLVER_USE_DOT_ALIAS_CATALOG
    return dotref::alias_catalog();
#else
    return {};
#endif
}

template <typename EntryT>
inline void add_match_if_new(std::vector<HelpMatch>& out,
                             std::unordered_set<std::string>& seen,
                             const std::string& name,
                             const std::string& catalog,
                             const std::string& summary,
                             bool alias_match,
                             bool exact,
                             int score)
{
    std::string dedupe_key = upper(catalog) + "|" + upper(name);
    if (seen.insert(dedupe_key).second)
    {
        out.push_back({name, catalog, summary, alias_match, exact, score});
    }
}

template <typename EntryT>
inline void scan_catalog_exact(const std::vector<EntryT>& catalog_entries,
                               const std::string& catalog_name,
                               const std::string& key,
                               std::vector<HelpMatch>& out,
                               std::unordered_set<std::string>& seen)
{
    for (const auto& e : catalog_entries)
    {
        if (upper(e.name) == key)
        {
            add_match_if_new<EntryT>(out, seen, e.name, catalog_name, e.summary,
                                     false, true,
                                     catalog_name == "dot" ? 1000 : 900);
        }
    }
}

template <typename EntryT>
inline void scan_catalog_partial(const std::vector<EntryT>& catalog_entries,
                                 const std::string& catalog_name,
                                 const std::string& raw_key,
                                 std::vector<HelpMatch>& out,
                                 std::unordered_set<std::string>& seen)
{
    for (const auto& e : catalog_entries)
    {
        const std::string name = e.name;

        if (starts_with_icase(name, raw_key))
        {
            add_match_if_new<EntryT>(out, seen, name, catalog_name, e.summary,
                                     false, false,
                                     catalog_name == "dot" ? 800 : 700);
        }
        else if (contains_icase(name, raw_key))
        {
            add_match_if_new<EntryT>(out, seen, name, catalog_name, e.summary,
                                     false, false,
                                     catalog_name == "dot" ? 500 : 400);
        }
    }
}

template <typename EntryT>
inline void scan_aliases_exact(const std::vector<AliasEntry>& aliases,
                               const std::vector<EntryT>& catalog_entries,
                               const std::string& catalog_name,
                               const std::string& key,
                               std::vector<HelpMatch>& out,
                               std::unordered_set<std::string>& seen)
{
    for (const auto& a : aliases)
    {
        if (upper(a.alias) != key)
            continue;

        for (const auto& e : catalog_entries)
        {
            if (upper(e.name) == upper(a.target))
            {
                add_match_if_new<EntryT>(out, seen, e.name, catalog_name, e.summary,
                                         true, true,
                                         catalog_name == "dot" ? 950 : 850);
                break;
            }
        }
    }
}

template <typename EntryT>
inline void scan_aliases_partial(const std::vector<AliasEntry>& aliases,
                                 const std::vector<EntryT>& catalog_entries,
                                 const std::string& catalog_name,
                                 const std::string& raw_key,
                                 std::vector<HelpMatch>& out,
                                 std::unordered_set<std::string>& seen)
{
    for (const auto& a : aliases)
    {
        if (!contains_icase(a.alias, raw_key))
            continue;

        for (const auto& e : catalog_entries)
        {
            if (upper(e.name) == upper(a.target))
            {
                add_match_if_new<EntryT>(out, seen, e.name, catalog_name, e.summary,
                                         true, false,
                                         catalog_name == "dot" ? 450 : 350);
                break;
            }
        }
    }
}

inline void rank_matches(std::vector<HelpMatch>& matches)
{
    std::stable_sort(matches.begin(), matches.end(),
        [](const HelpMatch& a, const HelpMatch& b)
        {
            if (a.score != b.score) return a.score > b.score;
            if (a.catalog != b.catalog) return a.catalog < b.catalog;
            return upper(a.name) < upper(b.name);
        });
}

inline HelpResult resolve(const std::string& term, Scope scope = Scope::Auto)
{
    HelpResult result;
    result.used_scope_filter = (scope != Scope::Auto);

    const std::string raw_key = trim(term);
    const std::string key = upper(raw_key);

    if (raw_key.empty())
        return result;

    const auto dot_catalog = dotref::catalog();
    const auto fox_catalog = foxref::catalog();
    const auto dot_aliases = dot_alias_catalog_fallback();
    const auto fox_aliases = fox_alias_catalog_fallback();

    std::unordered_set<std::string> seen;

    // Exact name matches first.
    if (scope == Scope::Auto || scope == Scope::Dot)
        scan_catalog_exact(dot_catalog, "dot", key, result.matches, seen);
    if (scope == Scope::Auto || scope == Scope::Fox)
        scan_catalog_exact(fox_catalog, "fox", key, result.matches, seen);

    // Exact aliases next.
    if (scope == Scope::Auto || scope == Scope::Dot)
        scan_aliases_exact(dot_aliases, dot_catalog, "dot", key, result.matches, seen);
    if (scope == Scope::Auto || scope == Scope::Fox)
        scan_aliases_exact(fox_aliases, fox_catalog, "fox", key, result.matches, seen);

    if (!result.matches.empty())
    {
        result.exact = true;

        bool has_dot_exact = false;
        bool has_fox_exact = false;
        for (const auto& m : result.matches)
        {
            if (m.catalog == "dot") has_dot_exact = true;
            if (m.catalog == "fox") has_fox_exact = true;
        }
        result.ambiguous = (scope == Scope::Auto && has_dot_exact && has_fox_exact);
        rank_matches(result.matches);
        return result;
    }

    // Prefix and contains matches.
    if (scope == Scope::Auto || scope == Scope::Dot)
        scan_catalog_partial(dot_catalog, "dot", raw_key, result.matches, seen);
    if (scope == Scope::Auto || scope == Scope::Fox)
        scan_catalog_partial(fox_catalog, "fox", raw_key, result.matches, seen);

    // Alias partials after name partials.
    if (scope == Scope::Auto || scope == Scope::Dot)
        scan_aliases_partial(dot_aliases, dot_catalog, "dot", raw_key, result.matches, seen);
    if (scope == Scope::Auto || scope == Scope::Fox)
        scan_aliases_partial(fox_aliases, fox_catalog, "fox", raw_key, result.matches, seen);

    rank_matches(result.matches);
    return result;
}

inline std::vector<HelpMatch> suggest(const std::string& term,
                                      Scope scope = Scope::Auto,
                                      size_t limit = 8)
{
    HelpResult r = resolve(term, scope);
    if (r.matches.size() > limit)
        r.matches.resize(limit);
    return r.matches;
}

} // namespace helpresolver

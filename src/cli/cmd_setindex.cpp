// src/cli/cmd_setindex.cpp
// SET INDEX TO <container>
//
// Current compile-safe contract:
// - INX: attach public container
// - CNX: attach public container
// - CDX: attach public container and validate derived LMDB env path
//
// Note:
// Full backend open/close through A.indexManager() requires the header that
// defines xindex::IndexManager, not just the forward declaration from xbase.hpp.

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

#include "xbase.hpp"
#include "cli/order_state.hpp"
#include "cli/path_resolver.hpp"

namespace fs = std::filesystem;

namespace {

static std::string up_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

static std::string lower_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

static bool ends_with_ci(const std::string& s, const char* suffix) {
    const std::string sl = lower_copy(s);
    const std::string su = lower_copy(std::string(suffix));
    if (sl.size() < su.size()) return false;
    return sl.compare(sl.size() - su.size(), su.size(), su) == 0;
}

static bool has_any_sep(const std::string& s) {
    return s.find('/') != std::string::npos || s.find('\\') != std::string::npos;
}

static bool looks_absolute(const std::string& s) {
    return (s.size() > 2 &&
            std::isalpha(static_cast<unsigned char>(s[0])) &&
            s[1] == ':') ||
           (!s.empty() && (s[0] == '/' || s[0] == '\\'));
}

static bool is_supported_index_ext(const fs::path& p) {
    const std::string ext = lower_copy(p.extension().string());
    return ext == ".inx" || ext == ".cnx" || ext == ".cdx";
}

static fs::path resolve_index_token(const std::string& tok) {
    if (ends_with_ci(tok, ".cdx.d")) {
        std::string s = tok;
        s.erase(s.size() - 2); // strip trailing ".d"
        return has_any_sep(s) || looks_absolute(s)
             ? fs::path(s)
             : dottalk::paths::resolve_index(s);
    }
    return dottalk::paths::resolve_index(tok);
}

static bool choose_container_path(const std::string& tok, fs::path& out_path) {
    fs::path base = resolve_index_token(tok);

    if (base.has_extension()) {
        if (!is_supported_index_ext(base)) return false;
        out_path = base;
        return true;
    }

    fs::path cdx = base; cdx.replace_extension(".cdx");
    fs::path cnx = base; cnx.replace_extension(".cnx");
    fs::path inx = base; inx.replace_extension(".inx");

    if (fs::exists(cdx)) { out_path = cdx; return true; }
    if (fs::exists(cnx)) { out_path = cnx; return true; }
    if (fs::exists(inx)) { out_path = inx; return true; }

    out_path = cdx;
    return true;
}

} // namespace

void cmd_SETINDEX(xbase::DbArea& A, std::istringstream& args)
{
    std::string tok;
    if (!(args >> tok)) {
        std::cout << "SET INDEX: missing filename.\n";
        return;
    }

    if (up_copy(tok) == "TO") {
        if (!(args >> tok)) {
            std::cout << "SET INDEX: missing filename.\n";
            return;
        }
    }

    fs::path p;
    if (!choose_container_path(tok, p)) {
        std::cout << "SET INDEX: unsupported index container: " << tok << "\n";
        std::cout << "Supported: .inx, .cnx, .cdx\n";
        return;
    }

    if (!fs::exists(p)) {
        std::cout << "SET INDEX: file not found: " << p.string() << "\n";
        return;
    }

    const std::string ext = lower_copy(p.extension().string());

    // Attach public container identity to order state.
    orderstate::setOrder(A, p.string());
    orderstate::setAscending(A, true);
    orderstate::setActiveTag(A, "");

    if (ext == ".cdx") {
        const fs::path env = dottalk::paths::resolve_lmdb_env_for_cdx(p);

        if (!fs::exists(env)) {
            std::cout << "SET INDEX: CDX container found but LMDB env missing\n";
            std::cout << "  Container: " << p.string() << "\n";
            std::cout << "  Expected : " << env.string() << "\n";
            std::cout << "Hint: run REINDEX CDX\n";
            return;
        }

        std::cout << "SET INDEX (CDX attached)\n";
        std::cout << "  Container: " << p.filename().string() << "\n";
        std::cout << "  LMDB env : " << env.string() << "\n";
        std::cout << "Use SET ORDER TO TAG <tag>\n";
        return;
    }

    if (ext == ".cnx") {
        std::cout << "SET INDEX (CNX attached)\n";
        std::cout << "  Container: " << p.filename().string() << "\n";
        std::cout << "Use SET ORDER TO TAG <tag>\n";
        return;
    }

    std::cout << "SET INDEX (INX attached)\n";
    std::cout << "  " << p.filename().string() << "\n";
}
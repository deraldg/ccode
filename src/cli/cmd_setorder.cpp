// src/cli/cmd_setorder.cpp
// FoxPro-style SET ORDER command with CNX/CDX-aware activation.
//
// Supported forms:
//   SET ORDER
//   SET ORDER 0
//   SET ORDER PHYSICAL | NATURAL | PHYS
//   SET ORDER <tag>
//   SET ORDER TAG <tag>
//   SET ORDER TAG <tag> IN <alias>
//   SET ORDER <container> [<tag>] [ASC|DESC|ASCEND|DESCEND|--asc|--desc]
//
// Notes:
// - For bare tag forms, an already-attached CNX/CDX container is preferred.
// - If no suitable container is attached, fallback is <table>.cdx.
// - For IN <alias>, target area is modified without changing the selected area.
// - Numeric tag-number orders remain reserved/minimal for now.
//
// CDX policy:
// - Public CDX container resolves under INDEXES.
// - LMDB backend resolves under LMDB.
// - This command validates both paths before attempting backend activation.

#include <sstream>
#include <string>
#include <iostream>
#include <filesystem>
#include <cctype>
#include <limits>
#include <vector>
#include <algorithm>

#include "xbase.hpp"
#include "xindex/index_manager.hpp"
#include "cdx/cdx.hpp"
#include "textio.hpp"
#include "cli/order_state.hpp"
#include "cli/path_resolver.hpp"
#include "cli/cmd_nav_move.hpp"

namespace fs = std::filesystem;

// ---------- helpers ----------------------------------------------------------

static inline std::string up_copy(std::string s) {
    for (auto& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

static inline std::string low_copy(std::string s) {
    for (auto& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

static inline bool ieq(const std::string& a, const char* b) {
    if (!b) return false;
    std::size_t n = 0;
    while (b[n] != '\0') ++n;
    if (a.size() != n) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

static inline bool is_integer(const std::string& s, int& outVal) {
    if (s.empty()) return false;
    std::size_t i = 0;
    if (s[0] == '+' || s[0] == '-') i = 1;
    if (i >= s.size()) return false;
    for (; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    try {
        long long v = std::stoll(s);
        if (v < std::numeric_limits<int>::min() ||
            v > std::numeric_limits<int>::max()) return false;
        outVal = static_cast<int>(v);
        return true;
    } catch (...) {
        return false;
    }
}

static inline bool ends_with_ci(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) return false;
    return up_copy(s.substr(s.size() - suffix.size())) == up_copy(suffix);
}

static inline bool contains_path_sep(const std::string& s) {
    return (s.find('/') != std::string::npos) || (s.find('\\') != std::string::npos);
}

static inline bool looks_absolute(const std::string& s) {
    return (s.size() > 2 && std::isalpha(static_cast<unsigned char>(s[0])) && s[1] == ':') ||
           (!s.empty() && (s[0] == '/' || s[0] == '\\'));
}

static bool is_direction_token(const std::string& s) {
    const std::string u = up_copy(s);
    return u == "ASC" || u == "DESC" || u == "ASCEND" || u == "DESCEND" ||
           u == "--ASC" || u == "--DESC";
}

static bool parse_direction_token(const std::string& s, bool& ascending) {
    const std::string u = up_copy(s);
    if (u == "ASC" || u == "ASCEND" || u == "--ASC") {
        ascending = true;
        return true;
    }
    if (u == "DESC" || u == "DESCEND" || u == "--DESC") {
        ascending = false;
        return true;
    }
    return false;
}

static inline bool looks_like_container_token(const std::string& s) {
    if (s.empty()) return false;
    if (contains_path_sep(s) || looks_absolute(s)) return true;
    if (ends_with_ci(s, ".CDX") || ends_with_ci(s, ".CNX") ||
        ends_with_ci(s, ".INX") || ends_with_ci(s, ".CDX.D")) {
        return true;
    }
    return false;
}

static fs::path resolve_index_path(const xbase::DbArea& area,
                                   const std::string& token,
                                   const std::string& defaultExt)
{
    if (token.empty()) {
        const std::string stem = !area.dbfBasename().empty() ? area.dbfBasename()
                                                             : area.logicalName();
        fs::path p = dottalk::paths::resolve_index(stem);
        if (!p.has_extension()) p.replace_extension(defaultExt);
        return p;
    }

    // normalize envdir token to container if user gives *.cdx.d
    if (ends_with_ci(token, ".CDX.D")) {
        std::string s = token;
        s.erase(s.size() - 2); // strip trailing ".d"
        fs::path p = (contains_path_sep(s) || looks_absolute(s))
                   ? fs::path(s)
                   : dottalk::paths::resolve_index(s);
        return p;
    }

    fs::path p = dottalk::paths::resolve_index(token);
    if (!p.has_extension()) p.replace_extension(defaultExt);
    return p;
}

static int resolve_area_index_by_name(xbase::XBaseEngine& eng, const std::string& tokRaw) {
    std::string tok = textio::trim(tokRaw);
    if (tok.empty()) return -1;

    int n = 0;
    if (is_integer(tok, n)) {
        if (n >= 0 && n < xbase::MAX_AREA) return n;
        return -1;
    }

    const std::string want = up_copy(tok);
    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        xbase::DbArea& A = eng.area(i);
        if (!A.isOpen()) continue;

        std::string name = A.name();
        std::replace(name.begin(), name.end(), '\\', '/');
        auto slash = name.find_last_of('/');
        if (slash != std::string::npos) name.erase(0, slash + 1);
        auto dot = name.find_last_of('.');
        if (dot != std::string::npos) name.erase(dot);
        name = up_copy(name);

        if (name == want) return i;
    }
    return -1;
}

extern "C" xbase::XBaseEngine* shell_engine();

static void clear_order_and_close_indexes(xbase::DbArea& area) {
    orderstate::clearOrder(area);
    area.indexManager().close();
}

// Stabilization fix:
// enter the order using the same backend scan path that LIST/SMARTLIST
// already proved correct. Fall back to nav TOP only if no CDX backend is active.
static void position_to_first_after_order_change(xbase::DbArea& area)
{
    try {
        auto& im = area.indexManager();

        if (!im.hasBackend() || !im.isCdx()) {
            cli::nav::go_endpoint(area, cli::nav::Endpoint::Top, "SET ORDER");
            return;
        }

        auto cur = im.scan(xindex::Key{}, xindex::Key{});
        if (!cur) {
            cli::nav::go_endpoint(area, cli::nav::Endpoint::Top, "SET ORDER");
            return;
        }

        xindex::Key k;
        xindex::RecNo r{0};

        if (cur->first(k, r)) {
            const int recno = static_cast<int>(r);
            if (recno > 0 && recno <= area.recCount()) {
                (void)area.gotoRec(recno);
                (void)area.readCurrent();
                return;
            }
        }

        cli::nav::go_endpoint(area, cli::nav::Endpoint::Top, "SET ORDER");
    } catch (...) {
        try {
            cli::nav::go_endpoint(area, cli::nav::Endpoint::Top, "SET ORDER");
        } catch (...) {
            // fail-safe: leave cursor where it is
        }
    }
}

// Validate against the same CDX source used by CDX INFO.
static bool cdx_has_tag(const std::string& container,
                        const std::string& wantedTag,
                        std::string& err)
{
    err.clear();

    cdxfile::CDXHandle* h = nullptr;

    if (!cdxfile::open(container, h)) {
        err = "unable to open CDX container.";
        return false;
    }

    std::vector<cdxfile::TagInfo> tags;
    if (!cdxfile::read_tagdir(h, tags)) {
        cdxfile::close(h);
        err = "unable to read CDX tag directory.";
        return false;
    }

    const std::string want = up_copy(wantedTag);

    for (const auto& t : tags) {
        if (up_copy(t.name) == want) {
            cdxfile::close(h);
            return true;
        }
    }

    cdxfile::close(h);
    err = "tag '" + wantedTag + "' not found in " + container;
    return false;
}

static bool cnx_has_tag(const xbase::DbArea& area,
                        const std::string& wantedTag)
{
    const auto& Fs = area.fields();
    const std::string want = up_copy(textio::trim(wantedTag));
    if (want.empty()) return false;

    if (!want.empty() && want[0] == '#') {
        try {
            int idx = std::stoi(want.substr(1));
            return idx >= 1 && idx <= static_cast<int>(Fs.size());
        } catch (...) {
            return false;
        }
    }

    for (const auto& f : Fs) {
        if (up_copy(textio::trim(f.name)) == want) {
            return true;
        }
    }

    return false;
}

static bool attached_container_is_tag_container(const xbase::DbArea& area) {
    if (!orderstate::hasOrder(area)) return false;
    return orderstate::isCdx(area) || orderstate::isCnx(area);
}

static std::string preferred_attached_container(const xbase::DbArea& area) {
    if (!attached_container_is_tag_container(area)) return std::string{};
    return orderstate::orderName(area);
}

static bool activate_cdx_on_area(xbase::DbArea& area,
                                 const std::string& container,
                                 const std::string& tag,
                                 bool ascending,
                                 std::string& err)
{
    err.clear();

    const fs::path container_path(container);
    const fs::path env_path = dottalk::paths::resolve_lmdb_env_for_cdx(container_path);

    if (!fs::exists(container_path)) {
        err = "openCdx: container not found: " + container_path.string();
        return false;
    }

    if (!fs::exists(env_path) || !fs::is_directory(env_path)) {
        err = "openCdx: LMDB env missing: " + env_path.string();
        return false;
    }

    orderstate::setOrder(area, container);
    orderstate::setActiveTag(area, tag);
    orderstate::setAscending(area, ascending);

    if (!area.indexManager().openCdx(container, tag, &err)) {
        if (err.empty()) {
            err = "openCdx: backend open() failed"
                  " [container=" + container_path.string() +
                  ", env=" + env_path.string() + "]";
        } else {
            err += " [container=" + container_path.string() +
                   ", env=" + env_path.string() + "]";
        }
        orderstate::clearOrder(area);
        area.indexManager().close();
        return false;
    }

    // openCdx() binds backend + tag, but does not position the record pointer.
    try {
        auto& im = area.indexManager();

        auto cur = im.seek(xindex::Key{});
        if (!cur) {
            cur = im.scan(xindex::Key{}, xindex::Key{});
        }

        if (cur) {
            xindex::Key k;
            xindex::RecNo r{0};

            const bool moved = ascending ? cur->first(k, r) : cur->last(k, r);
            if (moved) {
                const int recno = static_cast<int>(r);
                if (recno > 0 && recno <= area.recCount()) {
                    (void)area.gotoRec(recno);
                    (void)area.readCurrent();
                }
            }
        }
    } catch (...) {
        // fail-safe: leave cursor as-is
    }

    return true;
}

// ---------- command implementation -------------------------------------------

void cmd_SETORDER(xbase::DbArea& currentArea, std::istringstream& args)
{
    std::vector<std::string> toks;
    for (std::string t; args >> t; ) toks.push_back(t);

    // 1) No args -> show current order
    if (toks.empty()) {
        if (!orderstate::hasOrder(currentArea)) {
            std::cout << "SET ORDER: none (physical order).\n";
            return;
        }

        const std::string name = orderstate::orderName(currentArea);
        const bool asc = orderstate::isAscending(currentArea);
        const std::string tag = orderstate::activeTag(currentArea);

        std::string typeStr = "unknown";
        if (orderstate::isCnx(currentArea)) typeStr = "CNX";
        else if (orderstate::isCdx(currentArea)) typeStr = "CDX";
        else if (ends_with_ci(name, ".INX")) typeStr = "INX";

        std::cout << "SET ORDER: " << typeStr << " '" << name << "'";
        if (!tag.empty()) std::cout << " TAG '" << tag << "'";
        std::cout << " (" << (asc ? "ASC" : "DESC") << ")\n";
        return;
    }

    // Optional leading TO
    std::size_t i = 0;
    if (i < toks.size() && ieq(toks[i], "TO")) ++i;
    if (i >= toks.size()) {
        std::cout << "SET ORDER: missing target.\n";
        return;
    }

    // Optional trailing IN <alias>
    xbase::DbArea* target = &currentArea;
    if (toks.size() >= 2) {
        for (std::size_t k = i; k + 1 < toks.size(); ++k) {
            if (ieq(toks[k], "IN")) {
                auto* eng = shell_engine();
                if (!eng) {
                    std::cout << "SET ORDER: engine not available.\n";
                    return;
                }
                int areaIdx = resolve_area_index_by_name(*eng, toks[k + 1]);
                if (areaIdx < 0) {
                    std::cout << "SET ORDER: unknown area/alias: " << toks[k + 1] << "\n";
                    return;
                }
                target = &eng->area(areaIdx);
                toks.erase(toks.begin() + static_cast<std::ptrdiff_t>(k),
                           toks.begin() + static_cast<std::ptrdiff_t>(k + 2));
                break;
            }
        }
    }

    if (!target->isOpen()) {
        std::cout << "SET ORDER: no table open in target area.\n";
        return;
    }

    // Re-check after removing IN clause
    i = 0;
    if (i < toks.size() && ieq(toks[i], "TO")) ++i;
    if (i >= toks.size()) {
        std::cout << "SET ORDER: missing target.\n";
        return;
    }

    // 2) Physical / natural / 0
    {
        int n = 0;
        if (is_integer(toks[i], n)) {
            if (n == 0) {
                clear_order_and_close_indexes(*target);
                std::cout << "SET ORDER: cleared (physical order).\n";
                return;
            }
            std::cout << "SET ORDER: numeric tag orders not yet implemented (requested "
                      << n << ").\n";
            return;
        }

        const std::string u = up_copy(toks[i]);
        if (u == "PHYSICAL" || u == "NATURAL" || u == "PHYS") {
            clear_order_and_close_indexes(*target);
            std::cout << "SET ORDER: cleared (physical order).\n";
            return;
        }
    }

    // Optional direction token at end
    bool ascending = true;
    if (!toks.empty() && is_direction_token(toks.back())) {
        parse_direction_token(toks.back(), ascending);
        toks.pop_back();
    }

    // Recompute after popping direction
    i = 0;
    if (i < toks.size() && ieq(toks[i], "TO")) ++i;
    if (i >= toks.size()) {
        std::cout << "SET ORDER: missing target.\n";
        return;
    }

    std::string container;
    std::string tag;

    // 3) Fox-style TAG form:
    //    SET ORDER TO TAG LNAME
    if (ieq(toks[i], "TAG")) {
        if (i + 1 >= toks.size()) {
            std::cout << "SET ORDER: missing tag name after TAG.\n";
            return;
        }
        tag = up_copy(toks[i + 1]);

        // Prefer an already-attached CNX/CDX container first.
        container = preferred_attached_container(*target);
        if (container.empty()) {
            container = resolve_index_path(*target, "", ".cdx").string();
        }
    }
    // 4) Bare-tag shorthand:
    //    SET ORDER TO LNAME
    else if (i + 1 == toks.size()) {
        tag = up_copy(toks[i]);

        // Prefer an already-attached CNX/CDX container first.
        container = preferred_attached_container(*target);
        if (container.empty()) {
            container = resolve_index_path(*target, "", ".cdx").string();
        }
    }
    // 5) Explicit container + optional tag:
    //    SET ORDER TO students.cdx LNAME
    else {
        const std::string first = toks[i];
        const std::string second = toks[i + 1];

        container = resolve_index_path(*target, first, ".cdx").string();
        tag = up_copy(second);
    }

    if (container.empty()) {
        std::cout << "SET ORDER: unable to resolve container.\n";
        return;
    }

    if (tag.empty()) {
        std::cout << "SET ORDER: missing tag.\n";
        return;
    }

    if (!fs::exists(container)) {
        std::cout << "SET ORDER: file not found: " << container << "\n";
        return;
    }

    const bool isCdx = ends_with_ci(container, ".CDX");
    const bool isCnx = ends_with_ci(container, ".CNX");
    const bool isInx = ends_with_ci(container, ".INX");

    if (isInx) {
        std::cout << "SET ORDER: .INX activation is not yet implemented in this command.\n";
        return;
    }

    std::string err;
    bool ok = false;

    if (isCdx) {
        if (!cdx_has_tag(container, tag, err)) {
            std::cout << "SET ORDER: " << err << "\n";
            return;
        }

        ok = activate_cdx_on_area(*target, container, tag, ascending, err);
    } else if (isCnx) {
        // CNX remains state-driven for now; validate tag shape against area fields.
        if (!cnx_has_tag(*target, tag)) {
            std::cout << "SET ORDER: tag '" << tag << "' not available for CNX on current table.\n";
            return;
        }

        orderstate::setOrder(*target, container);
        orderstate::setActiveTag(*target, tag);
        orderstate::setAscending(*target, ascending);
        ok = true;
    } else {
        std::cout << "SET ORDER: unsupported index container: " << container << "\n";
        return;
    }

    if (!ok) {
        std::cout << "SET ORDER: " << (err.empty() ? "unable to activate order." : err) << "\n";
        return;
    }

    std::cout << "SET ORDER: CDX TAG '" << tag << "'"
              << " (" << (orderstate::isAscending(*target) ? "ASC" : "DESC") << ")\n";
}
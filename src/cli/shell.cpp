// src/cli/shell.cpp
// CLI shell — command registry + indexing hooks.
// Notes:
// - TVISION command is normally registered by the shell, but the FOXPRO GUI reuses
//   this registrar with include_ui_cmds=false to avoid GUI recursion.

#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <array>
#include <cctype>
#include <istream>

#include "command_registry.hpp"
#include "xbase.hpp"
#include "textio.hpp"
#include "colors.hpp"
#include "cmd_version.hpp"
#include "cmd_help.hpp"
#include "order_state.hpp"
#include "scan_state.hpp"
#include "dli/set_view.hpp"
#include "cmd_fox_palette_command.h"

// Do NOT include a header that pulls <tvision/tv.h> here; just forward-declare:
namespace xbase { class DbArea; }
void cmd_TVISION  (xbase::DbArea&, std::istringstream&);
void cmd_FOXPRO   (xbase::DbArea&, std::istringstream&);
void cmd_FOXTALK  (xbase::DbArea&, std::istringstream&);
void cmd_GENERIC  (xbase::DbArea&, std::istringstream&);
void cmd_TTESTAPP (xbase::DbArea&, std::istringstream&);
//void cmd_PALETTE  (xbase::DbArea&, std::istringstream&);
void cmd_RECORDVIEW (xbase::DbArea&, std::istringstream&);
void cmd_RECORD   (xbase::DbArea&, std::istringstream&);
void cmd_BROWSETV (xbase::DbArea&, std::istringstream&);
void cmd_FOX_PALETTE(xbase::DbArea&, std::istringstream&);

using xbase::DbArea;

// ---- Core command handlers (extern/forward decls) ----
void cmd_USE      (xbase::DbArea&, std::istringstream&);
void cmd_CLOSE    (xbase::DbArea&, std::istringstream&);
void cmd_LIST     (xbase::DbArea&, std::istringstream&);
void cmd_COPY     (xbase::DbArea&, std::istringstream&);
void cmd_EXPORT   (xbase::DbArea&, std::istringstream&);
void cmd_IMPORT   (xbase::DbArea&, std::istringstream&);
void cmd_APPEND   (xbase::DbArea&, std::istringstream&);

void cmd_TOP      (xbase::DbArea&, std::istringstream&);
void cmd_BOTTOM   (xbase::DbArea&, std::istringstream&);
void cmd_GOTO     (xbase::DbArea&, std::istringstream&);
void cmd_COUNT    (xbase::DbArea&, std::istringstream&);
void cmd_DISPLAY  (xbase::DbArea&, std::istringstream&);
void cmd_DELETE   (xbase::DbArea&, std::istringstream&);
void cmd_RECALL   (xbase::DbArea&, std::istringstream&);
void cmd_PACK     (xbase::DbArea&, std::istringstream&);
void cmd_COLOR    (xbase::DbArea&, std::istringstream&);
void cmd_FIELDS   (xbase::DbArea&, std::istringstream&);
void cmd_BROWSE   (xbase::DbArea&, std::istringstream&);
void cmd_BROWSETUI(xbase::DbArea&, std::istringstream&);

void browse_bind_invoke(void (*fn)(xbase::DbArea&, const std::string&));

void cmd_FIND     (xbase::DbArea&, std::istringstream&);
void cmd_SEEK     (xbase::DbArea&, std::istringstream&);
void cmd_SETORDER (xbase::DbArea&, std::istringstream&);
void cmd_SET      (xbase::DbArea&, std::istringstream&);

#if DOTTALK_WITH_INDEX
void cmd_INDEX    (xbase::DbArea&, std::istringstream&);
void cmd_REINDEX  (xbase::DbArea&, std::istringstream&);
void cmd_SETINDEX (xbase::DbArea&, std::istringstream&);
void cmd_ASCEND   (xbase::DbArea&, std::istringstream&);
void cmd_DESCEND  (xbase::DbArea&, std::istringstream&);
void cmd_CNX      (xbase::DbArea&, std::istringstream&);
#endif

void cmd_CLEAR    (xbase::DbArea&, std::istringstream&);
void cmd_CREATE   (xbase::DbArea&, std::istringstream&);
void cmd_DUMP     (xbase::DbArea&, std::istringstream&);
void cmd_EDIT     (xbase::DbArea&, std::istringstream&);
void cmd_LOCATE   (xbase::DbArea&, std::istringstream&);

void cmd_RECNO    (xbase::DbArea&, std::istringstream&);
void cmd_REFRESH  (xbase::DbArea&, std::istringstream&);
void cmd_REPLACE  (xbase::DbArea&, std::istringstream&);
void cmd_STATUS   (xbase::DbArea&, std::istringstream&);
void cmd_STRUCT   (xbase::DbArea&, std::istringstream&);
void cmd_SCHEMA   (xbase::DbArea&, std::istringstream&);
void cmd_ZAP      (xbase::DbArea&, std::istringstream&);

// System
void cmd_DIR      (xbase::DbArea&, std::istringstream&);
void cmd_BANG     (xbase::DbArea&, std::istringstream&);
void cmd_TEST     (xbase::DbArea&, std::istringstream&);
void cmd_FOXHELP  (xbase::DbArea&, std::istringstream&);
void cmd_SCAN     (xbase::DbArea&, std::istringstream&);
void cmd_ENDSCAN  (xbase::DbArea&, std::istringstream&);
void cmd_ECHO     (xbase::DbArea&, std::istringstream&);
void cmd_VERSION  (xbase::DbArea&, std::istringstream&);
void cmd_COLOR    (xbase::DbArea&, std::istringstream&);
void cmd_CALC     (xbase::DbArea&, std::istringstream&);
void cmd_BOOLEAN  (xbase::DbArea&, std::istringstream&);
void cmd_FORMULA  (xbase::DbArea&, std::istringstream&);

char prompt_char = '.';

// Scripting
void cmd_DOTSCRIPT(xbase::DbArea&, std::istringstream&);

// ---- Single-line dispatcher used by BROWSETUI bridge -----------------------
// Marked maybe_unused to avoid C4505 when binder is disabled.
[[maybe_unused]] static void browsetui_dispatch_line(xbase::DbArea& area, const std::string& rawLine)
{
    using namespace dli;

    size_t i = 0;
    while (i < rawLine.size() && std::isspace(static_cast<unsigned char>(rawLine[i]))) ++i;
    if (i < rawLine.size()) {
        if (rawLine[i] == '#') return;
        if (rawLine[i] == '/' && i + 1 < rawLine.size() && rawLine[i + 1] == '/') return;
    }

    std::string trimmed = textio::trim(rawLine);
    if (trimmed.empty()) return;

    std::istringstream tok(trimmed);
    std::string cmdToken;
    tok >> cmdToken;
    if (cmdToken.empty()) return;

    const std::string U = textio::up(cmdToken);

    if (scanblock::state().active && U != "ENDSCAN") {
        scanblock::state().lines.push_back(rawLine);
        return;
    }

    if (!registry().run(area, U, tok)) {
        std::cout << "Unknown command (from BROWSETUI): " << cmdToken << "\n";
    }
}

// ============================================================================
// NEW: shared engine access & centralized registration
// ============================================================================

// Global engine pointer; set by run_shell(), read by GUI.
static xbase::XBaseEngine* g_shell_engine = nullptr;

// Exported accessor for GUI.
extern "C" xbase::XBaseEngine* shell_engine() { return g_shell_engine; }

// Exported registrar used by both the CLI and the FOXPRO GUI.
// include_ui_cmds=false avoids registering GUI-launching commands inside the GUI.
extern "C" void register_shell_commands(xbase::XBaseEngine& eng, bool include_ui_cmds)
{
    using namespace dli;
    using xbase::DbArea;

    // --- Core / areas ---
    registry().add("USE",     [](DbArea& A, std::istringstream& S){ cmd_USE(A,S); });
    registry().add("CLOSE",   [](DbArea& A, std::istringstream& S){ cmd_CLOSE(A,S); });

    registry().add("SELECT",  [&](DbArea& /*A*/, std::istringstream& S){
        int n; S >> n;
        if (!S || n < 0 || n >= xbase::MAX_AREA) {
            std::cout << "Usage: SELECT <0.." << (xbase::MAX_AREA-1) << ">\n";
            return;
        }
        eng.selectArea(n);
        std::cout << "Selected area " << n << ".\n";
        DbArea& cur = eng.area(eng.currentArea());
        std::cout << "Current area: " << eng.currentArea() << "\n";
        if (cur.isOpen()) {
            std::cout << "  File: " << cur.name()
                      << "  Recs: " << cur.recCount()
                      << "  Recno: " << cur.recno() << "\n";
        } else {
            std::cout << "  (no file open)\n";
        }
    });

    registry().add("AREA",    [&](DbArea&, std::istringstream&){
        int i = eng.currentArea();
        DbArea& cur = eng.area(i);
        std::cout << "Current area: " << i << "\n";
        if (cur.isOpen()) {
            std::cout << "  File: " << cur.name()
                      << "  Recs: " << cur.recCount()
                      << "  Recno: " << cur.recno() << "\n";
            try {
                bool asc = orderstate::isAscending(cur);
                std::string tag = orderstate::hasOrder(cur) ? orderstate::orderName(cur) : std::string("(none)");
                std::cout << "  Order: " << (asc ? "ASCEND" : "DESCEND")
                          << "  Active tag: " << tag << "\n";
            } catch (...) {}
        } else {
            std::cout << "  (no file open)\n";
        }
    });

    // --- Record navigation & reporting ---
    registry().add("LIST",        [](DbArea& A, std::istringstream& S){ cmd_LIST(A,S); });
    registry().add("TOP",         [](DbArea& A, std::istringstream& S){ cmd_TOP(A,S); });
    registry().add("BOTTOM",      [](DbArea& A, std::istringstream& S){ cmd_BOTTOM(A,S); });
    registry().add("GOTO",        [](DbArea& A, std::istringstream& S){ cmd_GOTO(A,S); });
    registry().add("COUNT",       [](DbArea& A, std::istringstream& S){ cmd_COUNT(A,S); });
    registry().add("DISPLAY",     [](DbArea& A, std::istringstream& S){ cmd_DISPLAY(A,S); });
    registry().add("BROWSE",      [](DbArea& A, std::istringstream& S){ cmd_BROWSE(A,S); });
    registry().add("BROWSETUI",   [](DbArea& A, std::istringstream& S){ cmd_BROWSETUI(A,S); });
    registry().add("BT",          [](DbArea& A, std::istringstream& S){ cmd_BROWSETUI(A,S); });

    // --- TV/GUI launchers (guarded to prevent GUI recursion) ---
    if (include_ui_cmds) {
        registry().add("TVISION",      [](DbArea& A, std::istringstream& S){ cmd_TVISION(A,S); });
        registry().add("BTV",          [](DbArea& A, std::istringstream& S){ cmd_TVISION(A,S); });
        registry().add("FOXPRO",       [](DbArea& A, std::istringstream& S){ cmd_FOXPRO(A,S); });
        registry().add("FOXTALK",      [](DbArea& A, std::istringstream& S){ cmd_FOXTALK(A,S); });
        registry().add("GENERIC",      [](DbArea& A, std::istringstream& S){ cmd_GENERIC(A,S); });
        registry().add("TTEST",        [](DbArea& A, std::istringstream& S){ cmd_TTESTAPP(A,S); });
//      registry().add("PALETTE",      [](DbArea& A, std::istringstream& S){ cmd_PALETTE(A,S); });
        registry().add("BROWSETV",     [](DbArea& A, std::istringstream& S){ cmd_BROWSETV(A,S); });
        registry().add("RECORD",       [](DbArea& A, std::istringstream& S){ cmd_RECORD(A,S); });
        registry().add("RECORDVIEW",   [](DbArea& A, std::istringstream& S){ cmd_RECORDVIEW(A,S); });
        registry().add("FOXPALETTE",   [](DbArea& A, std::istringstream& S){ cmd_FOX_PALETTE(A,S); });
    }

    // --- Data I/O & maintenance ---
    registry().add("COPY",        [](DbArea& A, std::istringstream& S){ cmd_COPY(A,S);   });
    registry().add("EXPORT",      [](DbArea& A, std::istringstream& S){ cmd_EXPORT(A,S); });
    registry().add("IMPORT",      [](DbArea& A, std::istringstream& S){ cmd_IMPORT(A,S); });
    registry().add("APPEND",      [](DbArea& A, std::istringstream& S){ cmd_APPEND(A,S); });
    registry().add("DELETE",      [](DbArea& A, std::istringstream& S){ cmd_DELETE(A,S); });
    registry().add("RECALL",      [](DbArea& A, std::istringstream& S){ cmd_RECALL(A,S); });
    registry().add("UNDELETE",    [](DbArea& A, std::istringstream& S){ cmd_RECALL(A,S); });
    registry().add("PACK",        [](DbArea& A, std::istringstream& S){ cmd_PACK(A,S);   });
    registry().add("FIELDS",      [](DbArea& A, std::istringstream& S){ cmd_FIELDS(A,S); });

    // --- Search / order ---
    registry().add("FIND",        [](DbArea& A, std::istringstream& S){ cmd_FIND(A,S); });
    registry().add("SEEK",        [](DbArea& A, std::istringstream& S){ cmd_SEEK(A,S); });
#if DOTTALK_WITH_INDEX
    registry().add("CNX",         [](DbArea& A, std::istringstream& S){ cmd_CNX(A,S); });
    registry().add("INDEX",       [](DbArea& A, std::istringstream& S){ cmd_INDEX(A,S); });
    registry().add("REINDEX",     [](DbArea& A, std::istringstream& S){ cmd_REINDEX(A,S); });
    registry().add("SETINDEX",    [](DbArea& A, std::istringstream& S){ cmd_SETINDEX(A,S); });
    registry().add("ASCEND",      [](DbArea& A, std::istringstream& S){ cmd_ASCEND(A,S); });
    registry().add("DESCEND",     [](DbArea& A, std::istringstream& S){ cmd_DESCEND(A,S); });
    registry().add("SETORDER",    [](DbArea& A, std::istringstream& S){ cmd_SETORDER(A,S); });
    registry().add("SET",         [](DbArea& A, std::istringstream& S){ cmd_SET(A,S); });
#endif

    // --- Misc / maintenance ---
    registry().add("CLEAR",       [](DbArea& A, std::istringstream& S){ cmd_CLEAR(A,S); });
    registry().add("CREATE",      [](DbArea& A, std::istringstream& S){ cmd_CREATE(A,S); });
    registry().add("DUMP",        [](DbArea& A, std::istringstream& S){ cmd_DUMP(A,S); });
    registry().add("EDIT",        [](DbArea& A, std::istringstream& S){ cmd_EDIT(A,S); });
    registry().add("LOCATE",      [](DbArea& A, std::istringstream& S){ cmd_LOCATE(A,S); });
    registry().add("RECNO",       [](DbArea& A, std::istringstream& S){ cmd_RECNO(A,S); });
    registry().add("REFRESH",     [](DbArea& A, std::istringstream& S){ cmd_REFRESH(A,S); });
    registry().add("REPLACE",     [](DbArea& A, std::istringstream& S){ cmd_REPLACE(A,S); });
    registry().add("STATUS",      [](DbArea& A, std::istringstream& S){ cmd_STATUS(A,S); });
    registry().add("STRUCT",      [](DbArea& A, std::istringstream& S){ cmd_STRUCT(A,S); });
    registry().add("SCHEMA",      [](DbArea& A, std::istringstream& S){ cmd_SCHEMA(A,S); });
    registry().add("ZAP",         [](DbArea& A, std::istringstream& S){ cmd_ZAP(A,S); });

    // --- System / utilities ---
    registry().add("DIR",         [](DbArea& A, std::istringstream& S){ cmd_DIR(A,S);  });
    registry().add("COLOR",       [](DbArea& A, std::istringstream& S){ cmd_COLOR(A,S);  });
    registry().add("!",           [](DbArea& A, std::istringstream& S){ cmd_BANG(A,S); });
    registry().add("?",           [](DbArea& A, std::istringstream& S){ cmd_FORMULA(A,S); });
    registry().add("CALC",        [](DbArea& A, std::istringstream& S){ cmd_CALC(A,S); });
    registry().add("BOOLEAN",     [](DbArea& A, std::istringstream& S){ cmd_BOOLEAN(A,S); });  // FIX: comma added
    registry().add("FORMULA",     [](DbArea& A, std::istringstream& S){ cmd_FORMULA(A,S); });  // FIX: comma added

    // --- HELP and friends ---
    registry().add("SCAN",        [](DbArea& A, std::istringstream& S){ cmd_SCAN(A,S); });
    registry().add("ENDSCAN",     [](DbArea& A, std::istringstream& S){ cmd_ENDSCAN(A,S); });
    registry().add("HELP",        [](DbArea& A, std::istringstream& S){ cmd_HELP(A,S); });
    registry().add("H",           [](DbArea& A, std::istringstream& S){ cmd_HELP(A,S); });
    registry().add("TEST",        [](DbArea& A, std::istringstream& S){ cmd_TEST(A,S); });
    registry().add("FOXHELP",     [](DbArea& A, std::istringstream& S){ cmd_FOXHELP(A,S); });
    registry().add("FH",          [](DbArea& A, std::istringstream& S){ cmd_FOXHELP(A,S); });

    // --- Scripting ---
    registry().add("DOTSCRIPT",   [](DbArea& A, std::istringstream& S){ cmd_DOTSCRIPT(A,S); });
    registry().add("RUN",         [](DbArea& A, std::istringstream& S){ cmd_DOTSCRIPT(A,S); });
    registry().add("DO",          [](DbArea& A, std::istringstream& S){ cmd_DOTSCRIPT(A,S); });
    registry().add("ECHO",        [](DbArea& A, std::istringstream& S){ cmd_ECHO(A,S); });
    registry().add("VERSION",     [](DbArea& A, std::istringstream& S){ cmd_VERSION(A,S); });
}

// ============================================================================
// Interactive shell (unchanged UX; now uses the centralized registrar)
// ============================================================================
int run_shell()
{
    using namespace xbase;
    using namespace dli;

    colors::applyTheme(colors::Theme::Green);

    XBaseEngine eng;
    eng.selectArea(0);
    g_shell_engine = &eng; // expose to GUI

    // Register everything for CLI, including GUI launchers.
    register_shell_commands(eng, /*include_ui_cmds=*/true);

    // browse_bind_invoke(&browsetui_dispatch_line);

    clear();

    std::cout << "DotTalk++ type HELP. USE, SELECT <n>, AREA, COLOR <GREEN|AMBER|DEFAULT>, QUIT.\n";

    std::string line;
    while (true) {
        std::cout << prompt_char << " ";
        if (!std::getline(std::cin, line)) break;

        size_t i = 0;
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
        if (i < line.size()) {
            if (line[i] == '#') continue;
            if (line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/') continue;
        }

        const std::string rawLine = line;
        std::string trimmed = textio::trim(line);
        if (trimmed.empty()) continue;

        std::istringstream tok(trimmed);
        std::string cmdToken;
        tok >> cmdToken;
        if (cmdToken.empty()) continue;
        const std::string U = textio::up(cmdToken);

        if (scanblock::state().active && U != "ENDSCAN") {
            scanblock::state().lines.push_back(rawLine);
            continue;
        }

        if (U == "QUIT" || U == "EXIT") break;

        DbArea& cur = eng.area(eng.currentArea());
        if (!dli::registry().run(cur, U, tok)) {
            std::cout << "Unknown command: " << cmdToken << "\n";
        }
    }

    g_shell_engine = nullptr; // cleanup before exit
    return 0;
}

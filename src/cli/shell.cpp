// shell.cpp   restored command registry + indexing hooks
#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <array>
#include <cctype>          // for std::isspace

#include "command_registry.hpp"
#include "xbase.hpp"
#include "textio.hpp"
#include "colors.hpp"
#include "cmd_version.hpp"
#include "cmd_help.hpp"    // routed HELP lives in its own module
#include "order_state.hpp"
#include "scan_state.hpp"

using xbase::DbArea;

// ---- Core command handlers (extern/forward decls) ----
void cmd_USE   (xbase::DbArea&, std::istringstream&);
void cmd_CLOSE (xbase::DbArea&, std::istringstream&);
void cmd_LIST  (xbase::DbArea&, std::istringstream&);
void cmd_COPY  (xbase::DbArea&, std::istringstream&);
void cmd_EXPORT(xbase::DbArea&, std::istringstream&);
void cmd_IMPORT(xbase::DbArea&, std::istringstream&);
void cmd_APPEND(xbase::DbArea&, std::istringstream&);

void cmd_TOP   (xbase::DbArea&, std::istringstream&);
void cmd_BOTTOM(xbase::DbArea&, std::istringstream&);
void cmd_GOTO  (xbase::DbArea&, std::istringstream&);
void cmd_COUNT (xbase::DbArea&, std::istringstream&);
void cmd_DISPLAY(xbase::DbArea&, std::istringstream&);
void cmd_DELETE(xbase::DbArea&, std::istringstream&);
void cmd_RECALL(xbase::DbArea&, std::istringstream&);
void cmd_PACK  (xbase::DbArea&, std::istringstream&);
void cmd_COLOR (xbase::DbArea&, std::istringstream&);
void cmd_FIELDS(xbase::DbArea&, std::istringstream&);

// FIND/SEEK are available either way; SEEK may route to index or linear
void cmd_FIND(xbase::DbArea&, std::istringstream& iss);
void cmd_SEEK (xbase::DbArea&, std::istringstream&);
void cmd_SETORDER (xbase::DbArea&, std::istringstream&);
void cmd_SET  (xbase::DbArea&, std::istringstream&);

// ---- Indexing command handlers (compiled when enabled) ----
#if DOTTALK_WITH_INDEX
void cmd_INDEX    (xbase::DbArea&, std::istringstream&);   // e.g., INDEX ON <expr> TAG <tag> [ASC|DESC]
void cmd_REINDEX  (xbase::DbArea&, std::istringstream&); 
void cmd_SETINDEX (xbase::DbArea&, std::istringstream&);   // e.g., SET INDEX TO <tag|path>
void cmd_ASCEND   (xbase::DbArea&, std::istringstream&);
void cmd_DESCEND  (xbase::DbArea&, std::istringstream&);
#endif

// Already present ones...
void cmd_CLEAR(xbase::DbArea&, std::istringstream&);
void cmd_CREATE(xbase::DbArea&, std::istringstream&);
void cmd_DUMP(xbase::DbArea&, std::istringstream&);
void cmd_EDIT(xbase::DbArea&, std::istringstream&);
void cmd_LOCATE(xbase::DbArea&, std::istringstream&);

void cmd_RECNO(xbase::DbArea&, std::istringstream&);
void cmd_REFRESH(xbase::DbArea&, std::istringstream&);
void cmd_REPLACE(xbase::DbArea&, std::istringstream&);
void cmd_STATUS(xbase::DbArea&, std::istringstream&);
void cmd_STRUCT(xbase::DbArea&, std::istringstream&);
void cmd_ZAP(xbase::DbArea&, std::istringstream&);

// System
void cmd_DIR      (xbase::DbArea&, std::istringstream&);
void cmd_BANG     (xbase::DbArea&, std::istringstream&);
void cmd_TEST     (xbase::DbArea&, std::istringstream&);
void cmd_FOXHELP  (xbase::DbArea&, std::istringstream&);
void cmd_SCAN     (xbase::DbArea&, std::istringstream&);
void cmd_ENDSCAN  (xbase::DbArea&, std::istringstream&);
void cmd_ECHO     (xbase::DbArea&, std::istringstream&);


// Scripting
void cmd_DOTSCRIPT(xbase::DbArea&, std::istringstream&);  // DotScript runner

int run_shell()
{
    using namespace xbase;
    using namespace dli;

    // Theme - compile err
    colors::applyTheme(colors::Theme::Green);
 // struct ResetAtExit { ~ResetAtExit(){ colors::reset(); } } _reset_guard;
 // dlc::colors::applyTheme(dlc::colors::Theme:Amber);

    XBaseEngine eng;
    eng.selectArea(0);

    // --- Register commands -------------------------------------------------
    dli::registry().add("USE",     [](DbArea& A, std::istringstream& S){ cmd_USE(A,S); });

    dli::registry().add("SELECT",  [&](DbArea& /*A*/, std::istringstream& S){
        int n; S >> n;
        if (!S || n < 0 || n >= MAX_AREA) {
            std::cout << "Usage: SELECT <0.." << (MAX_AREA-1) << ">\n";
            return;
        }
        eng.selectArea(n);
        std::cout << "Selected area " << n << ".\n";
        // Print AREA snapshot after switch
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

    dli::registry().add("AREA",    [&](DbArea&, std::istringstream&){
        int i = eng.currentArea();
        DbArea& cur = eng.area(i);
        std::cout << "Current area: " << i << "\n";
        if (cur.isOpen()) {
            std::cout << "  File: " << cur.name()
                      << "  Recs: " << cur.recCount()
                      << "  Recno: " << cur.recno() << "\n";
            // Order state snapshot
            try {
                bool asc = orderstate::isAscending(cur);
                std::string tag = orderstate::hasOrder(cur) ? orderstate::orderName(cur) : std::string("(none)");
                std::cout << "  Order: " << (asc ? "ASCEND" : "DESCEND")
                          << "  Active tag: " << tag << "\n";
            } catch (...) {
                // orderstate not available; ignore
            }
        } else {
            std::cout << "  (no file open)\n";
        }
    });

    dli::registry().add("CLOSE",   [](DbArea& A, std::istringstream& S){ cmd_CLOSE(A,S); });

    // --- Record navigation & reporting ---
    dli::registry().add("LIST",    [](DbArea& A, std::istringstream& S){ cmd_LIST(A,S); });
    dli::registry().add("TOP",     [](DbArea& A, std::istringstream& S){ cmd_TOP(A,S); });
    dli::registry().add("BOTTOM",  [](DbArea& A, std::istringstream& S){ cmd_BOTTOM(A,S); });
    dli::registry().add("GOTO",    [](DbArea& A, std::istringstream& S){ cmd_GOTO(A,S); });
    dli::registry().add("COUNT",   [](DbArea& A, std::istringstream& S){ cmd_COUNT(A,S); });
    dli::registry().add("DISPLAY", [](DbArea& A, std::istringstream& S){ cmd_DISPLAY(A,S); });

    // --- Data I/O & maintenance ---
    dli::registry().add("COPY",    [](DbArea& A, std::istringstream& S){ cmd_COPY(A,S); });
    dli::registry().add("EXPORT",  [](DbArea& A, std::istringstream& S){ cmd_EXPORT(A,S); });
    dli::registry().add("IMPORT",  [](DbArea& A, std::istringstream& S){ cmd_IMPORT(A,S); });
    dli::registry().add("APPEND",  [](DbArea& A, std::istringstream& S){ cmd_APPEND(A,S); });
    dli::registry().add("DELETE",  [](DbArea& A, std::istringstream& S){ cmd_DELETE(A,S); });
    dli::registry().add("RECALL",  [](DbArea& A, std::istringstream& S){ cmd_RECALL(A,S); });
    dli::registry().add("UNDELETE",[](DbArea& A, std::istringstream& S){ cmd_RECALL(A,S); });
    dli::registry().add("PACK",    [](DbArea& A, std::istringstream& S){ cmd_PACK(A,S); });
    dli::registry().add("FIELDS",  [](DbArea& A, std::istringstream& S){ cmd_FIELDS(A,S); });

    // --- Search ---
    dli::registry().add("FIND", cmd_FIND);
    dli::registry().add("SEEK", cmd_SEEK);

    // --- UI/utility ---
    dli::registry().add("COLOR",   [](DbArea& A, std::istringstream& S){ cmd_COLOR(A,S); });
    dli::registry().add("VERSION", [](DbArea& A, std::istringstream& S){ cmd_VERSION(A,S); });

    // --- Indexing (only when enabled) ---
#if DOTTALK_WITH_INDEX
    dli::registry().add("INDEX",       [](DbArea& A, std::istringstream& S){ cmd_INDEX(A,S); });
    dli::registry().add("REINDEX",     [](DbArea& A, std::istringstream& S){ cmd_REINDEX(A,S); });
    dli::registry().add("SETINDEX",    [](DbArea& A, std::istringstream& S){ cmd_SETINDEX(A,S); });
    dli::registry().add("SET INDEX",   [](DbArea& A, std::istringstream& S){ cmd_SETINDEX(A,S); });   // spaced key support
    dli::registry().add("SET INDEX TO",[](DbArea& A, std::istringstream& S){ cmd_SETINDEX(A,S); });   // nicety
    dli::registry().add("ASCEND",      [](DbArea& A, std::istringstream& S){ cmd_ASCEND(A,S); });
    dli::registry().add("DESCEND",     [](DbArea& A, std::istringstream& S){ cmd_DESCEND(A,S); });
    dli::registry().add("SETORDER",    [](DbArea& A, std::istringstream& S){ cmd_SETORDER(A,S); });
    dli::registry().add("SET ORDER",   [](DbArea& A, std::istringstream& S){ cmd_SETORDER(A,S); });
    dli::registry().add("SET",         [](DbArea& A, std::istringstream& S){ cmd_SET(A,S); });
#endif

    dli::registry().add("CLEAR",        [](DbArea& A, std::istringstream& S){ cmd_CLEAR(A,S); });
    dli::registry().add("CREATE",       [](DbArea& A, std::istringstream& S){ cmd_CREATE(A,S); });
    dli::registry().add("DUMP",         [](DbArea& A, std::istringstream& S){ cmd_DUMP(A,S); });
    dli::registry().add("EDIT",         [](DbArea& A, std::istringstream& S){ cmd_EDIT(A,S); });
    dli::registry().add("LOCATE",       [](DbArea& A, std::istringstream& S){ cmd_LOCATE(A,S); });
    dli::registry().add("RECNO",        [](DbArea& A, std::istringstream& S){ cmd_RECNO(A,S); });
    dli::registry().add("REFRESH",      [](DbArea& A, std::istringstream& S){ cmd_REFRESH(A,S); });
    dli::registry().add("REPLACE",      [](DbArea& A, std::istringstream& S){ cmd_REPLACE(A,S); });
    dli::registry().add("STATUS",       [](DbArea& A, std::istringstream& S){ cmd_STATUS(A,S); });
    dli::registry().add("STRUCT",       [](DbArea& A, std::istringstream& S){ cmd_STRUCT(A,S); });
    dli::registry().add("ZAP",          [](DbArea& A, std::istringstream& S){ cmd_ZAP(A,S); });

    // System / utilities
    dli::registry().add("DIR",     [](DbArea& A, std::istringstream& S){ cmd_DIR(A,S);   });
    dli::registry().add("!",       [](DbArea& A, std::istringstream& S){ cmd_BANG(A,S);  });

    // ---- HELP is now a first-class command module ----
    dli::registry().add("SCAN",    [](DbArea& A, std::istringstream& S){ cmd_SCAN(A,S); });
    dli::registry().add("ENDSCAN", [](DbArea& A, std::istringstream& S){ cmd_ENDSCAN(A,S); });
    dli::registry().add("HELP",    [](DbArea& A, std::istringstream& S){ cmd_HELP(A,S); });
    dli::registry().add("?",       [](DbArea& A, std::istringstream& S){ cmd_HELP(A,S); });
    dli::registry().add("TEST",    [](DbArea& A, std::istringstream& S){ cmd_TEST(A,S); });
    dli::registry().add("FOXHELP", [](DbArea& A, std::istringstream& S){ cmd_FOXHELP(A,S); });
    dli::registry().add("FH",      [](DbArea& A, std::istringstream& S){ cmd_FOXHELP(A,S); });

    // --- In the registration block with other commands ---
    dli::registry().add("DOTSCRIPT", [](DbArea& A, std::istringstream& S){ cmd_DOTSCRIPT(A,S); });
    dli::registry().add("RUN",       [](DbArea& A, std::istringstream& S){ cmd_DOTSCRIPT(A,S); });
    dli::registry().add("DO",        [](DbArea& A, std::istringstream& S){ cmd_DOTSCRIPT(A,S); });
    dli::registry().add("ECHO",      [](DbArea& A, std::istringstream& S){ cmd_ECHO(A,S); });


    // debug
    // std::cout << "[wire] FIND -> " << (void*)&cmd_FIND << "\n";
    // std::cout << "[wire] SEEK -> " << (void*)&cmd_SEEK  << "\n";

    // --- Startup banner ---
    std::cout << "DotTalk++ type HELP. USE, SELECT <n>, AREA, COLOR <GREEN|AMBER|DEFAULT>, QUIT.\n";

    // --- REPL --------------------------------------------------------------
    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;

        // 1) Skip whole-line comments (allow leading whitespace)
        {
            size_t i = 0;
            while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
            if (i < line.size()) {
                if (line[i] == '#') continue;
                if (line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/') continue;
            }
        }

        // Keep the raw user line for SCAN buffering; also get a trimmed copy for tokenizing
        const std::string rawLine = line;
        std::string trimmed = textio::trim(line);
        if (trimmed.empty()) continue;

        // 2) Pull first token (command) from the trimmed line
        std::istringstream tok(trimmed);
        std::string cmdToken;
        tok >> cmdToken;
        if (cmdToken.empty()) continue;
        const std::string U = textio::up(cmdToken);

        // 3) SCAN buffering guard: if active and not ENDSCAN, accumulate raw line and continue
        if (scanblock::state().active && U != "ENDSCAN") {
            scanblock::state().lines.push_back(rawLine);
            continue; // do NOT dispatch yet
        }

        // 4) QUIT / EXIT
        if (U == "QUIT" || U == "EXIT") break;

        // 5) Dispatch: 'tok' is already positioned after the command token
        DbArea& cur = eng.area(eng.currentArea());
        if (!dli::registry().run(cur, U, tok)) {
            std::cout << "Unknown command: " << cmdToken << "\n";
        }
    }

    return 0;
}


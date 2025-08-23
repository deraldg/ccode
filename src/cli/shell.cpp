// shell.cpp   restored command registry + indexing hooks
#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <array>

#include "command_registry.hpp"
#include "xbase.hpp"
#include "textio.hpp"
#include "colors.hpp"
#include "cmd_version.hpp"
#include "cmd_help.hpp"   // <-- NEW: routed HELP lives in its own module
#include "order_state.hpp"


//extern void cmd_FIND(xbase::DbArea&, std::istringstream&);
//extern void cmd_SEEK(xbase::DbArea&, std::istringstream&);

using xbase::DbArea;

// ---- Core command handlers (extern/forward decls) ----
void cmd_USE   (xbase::DbArea&, std::istringstream&);
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

// ---- Indexing command handlers (compiled when enabled) ----
#if DOTTALK_WITH_INDEX
void cmd_INDEX    (xbase::DbArea&, std::istringstream&);   // e.g., INDEX ON <expr> TAG <tag> [ASC|DESC]
void cmd_SETINDEX (xbase::DbArea&, std::istringstream&);   // e.g., SET INDEX TO <tag|path>
void cmd_ASCEND   (xbase::DbArea&, std::istringstream&);
void cmd_DESCEND  (xbase::DbArea&, std::istringstream&);
#endif

// Already present ones...
//void cmd_APPEND_BLANK(xbase::DbArea&, std::istringstream&);
void cmd_CLEAR(xbase::DbArea&, std::istringstream&);
void cmd_CREATE(xbase::DbArea&, std::istringstream&);
void cmd_DUMP(xbase::DbArea&, std::istringstream&);
void cmd_EDIT(xbase::DbArea&, std::istringstream&);
//void cmd_LOCATE(xbase::DbArea&, std::istringstream&);
void cmd_RECNO(xbase::DbArea&, std::istringstream&);
void cmd_REFRESH(xbase::DbArea&, std::istringstream&);
void cmd_REPLACE(xbase::DbArea&, std::istringstream&);
void cmd_STATUS(xbase::DbArea&, std::istringstream&);
void cmd_STRUCT(xbase::DbArea&, std::istringstream&);
//void cmd_ZAP(xbase::DbArea&, std::istringstream&);

// New ones
void cmd_DIR   (xbase::DbArea&, std::istringstream&);
void cmd_BANG  (xbase::DbArea&, std::istringstream&);
//void cmd_RECNO (xbase::DbArea&, std::istringstream&);

int run_shell()
{
    using namespace xbase;
    using namespace cli;

    // Theme
    colors::applyTheme(colors::Theme::Green);
    struct ResetAtExit { ~ResetAtExit(){ colors::reset(); } } _reset_guard;

    XBaseEngine eng;
    eng.selectArea(0);

    // --- Register commands -------------------------------------------------
    cli::registry().add("USE",     [](DbArea& A, std::istringstream& S){ cmd_USE(A,S); });

    cli::registry().add("SELECT",  [&](DbArea& /*A*/, std::istringstream& S){
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

    cli::registry().add("AREA",    [&](DbArea&, std::istringstream&){
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

    // --- Record navigation & reporting ---
    cli::registry().add("LIST",    [](DbArea& A, std::istringstream& S){ cmd_LIST(A,S); });
    cli::registry().add("TOP",     [](DbArea& A, std::istringstream& S){ cmd_TOP(A,S); });
    cli::registry().add("BOTTOM",  [](DbArea& A, std::istringstream& S){ cmd_BOTTOM(A,S); });
    cli::registry().add("GOTO",    [](DbArea& A, std::istringstream& S){ cmd_GOTO(A,S); });
    cli::registry().add("COUNT",   [](DbArea& A, std::istringstream& S){ cmd_COUNT(A,S); });
    cli::registry().add("DISPLAY", [](DbArea& A, std::istringstream& S){ cmd_DISPLAY(A,S); });

    // --- Data I/O & maintenance ---
    cli::registry().add("COPY",    [](DbArea& A, std::istringstream& S){ cmd_COPY(A,S); });
    cli::registry().add("EXPORT",  [](DbArea& A, std::istringstream& S){ cmd_EXPORT(A,S); });
    cli::registry().add("IMPORT",  [](DbArea& A, std::istringstream& S){ cmd_IMPORT(A,S); });
    cli::registry().add("APPEND",  [](DbArea& A, std::istringstream& S){ cmd_APPEND(A,S); });
    cli::registry().add("DELETE",  [](DbArea& A, std::istringstream& S){ cmd_DELETE(A,S); });
    cli::registry().add("RECALL",  [](DbArea& A, std::istringstream& S){ cmd_RECALL(A,S); });
    cli::registry().add("UNDELETE",[](DbArea& A, std::istringstream& S){ cmd_RECALL(A,S); });
    cli::registry().add("PACK",    [](DbArea& A, std::istringstream& S){ cmd_PACK(A,S); });
    cli::registry().add("FIELDS",  [](DbArea& A, std::istringstream& S){ cmd_FIELDS(A,S); });

    // --- Search ---
//  cli::registry().add("FIND",    [](DbArea& A, std::istringstream& S){ cmd_FIND(A,S); });
//  cli::registry().add("SEEK",    [](DbArea& A, std::istringstream& S){ cmd_SEEK(A,S); });

    cli::registry().add("FIND", cmd_FIND);
    cli::registry().add("SEEK", cmd_SEEK);

    // --- UI/utility ---
    cli::registry().add("COLOR",   [](DbArea& A, std::istringstream& S){ cmd_COLOR(A,S); });
    cli::registry().add("VERSION", [](DbArea& A, std::istringstream& S){ cmd_VERSION(A,S); });

    // --- Indexing (only when enabled) ---
#if DOTTALK_WITH_INDEX
    cli::registry().add("INDEX",       [](DbArea& A, std::istringstream& S){ cmd_INDEX(A,S); });
    cli::registry().add("SETINDEX",    [](DbArea& A, std::istringstream& S){ cmd_SETINDEX(A,S); });
    cli::registry().add("SET INDEX",   [](DbArea& A, std::istringstream& S){ cmd_SETINDEX(A,S); });   // spaced key support
    cli::registry().add("SET INDEX TO",[](DbArea& A, std::istringstream& S){ cmd_SETINDEX(A,S); });   // nicety
    cli::registry().add("ASCEND",      [](DbArea& A, std::istringstream& S){ cmd_ASCEND(A,S); });
    cli::registry().add("DESCEND",     [](DbArea& A, std::istringstream& S){ cmd_DESCEND(A,S); });
    cli::registry().add("SETORDER",    [](DbArea& A, std::istringstream& S){ cmd_SETORDER(A,S); });
    cli::registry().add("SET ORDER",   [](DbArea& A, std::istringstream& S){ cmd_SETORDER(A,S); });
#endif

//  cli::registry().add("APPEND BLANK", [](DbArea& A, std::istringstream& S){ cmd_APPEND_BLANK(A,S); });
    cli::registry().add("CLEAR",        [](DbArea& A, std::istringstream& S){ cmd_CLEAR(A,S); });
//  cli::registry().add("CLS",          cmd_CLEAR);  // single, not duplicated
    cli::registry().add("CREATE",       [](DbArea& A, std::istringstream& S){ cmd_CREATE(A,S); });
    cli::registry().add("DUMP",         [](DbArea& A, std::istringstream& S){ cmd_DUMP(A,S); });
    cli::registry().add("EDIT",         [](DbArea& A, std::istringstream& S){ cmd_EDIT(A,S); });
//  cli::registry().add("LOCATE",       [](DbArea& A, std::istringstream& S){ cmd_LOCATE(A,S); });
    cli::registry().add("RECNO",        [](DbArea& A, std::istringstream& S){ cmd_RECNO(A,S); });
    cli::registry().add("REFRESH",      [](DbArea& A, std::istringstream& S){ cmd_REFRESH(A,S); });
    cli::registry().add("REPLACE",      [](DbArea& A, std::istringstream& S){ cmd_REPLACE(A,S); });
    cli::registry().add("STATUS",       [](DbArea& A, std::istringstream& S){ cmd_STATUS(A,S); });
    cli::registry().add("STRUCT",       [](DbArea& A, std::istringstream& S){ cmd_STRUCT(A,S); });
//  cli::registry().add("ZAP",          [](DbArea& A, std::istringstream& S){ cmd_ZAP(A,S); });

// System / utilities
    cli::registry().add("DIR",     [](DbArea& A, std::istringstream& S){ cmd_DIR(A,S);   });
    cli::registry().add("!",       [](DbArea& A, std::istringstream& S){ cmd_BANG(A,S);  });

    // ---- HELP is now a first-class command module ----
    cli::registry().add("HELP",    [](DbArea& A, std::istringstream& S){ cmd_HELP(A,S); });  // <  NEW
    cli::registry().add("?",       [](DbArea& A, std::istringstream& S){ cmd_HELP(A,S); });  // <  optional alias

//debug
    std::cout << "[wire] FIND -> " << (void*)&cmd_FIND << "\n";
    std::cout << "[wire] SEEK -> " << (void*)&cmd_SEEK  << "\n";


    // --- Startup CLS and banner ---
    //  textio::cls();  // clear once when the app starts
    std::cout << "DotTalk++ type HELP. USE, SELECT <n>, AREA, COLOR <GREEN|AMBER|DEFAULT>, QUIT.\n";

    // --- REPL --------------------------------------------------------------
    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;

        line = textio::trim(line);
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string cmd; iss >> cmd;
        std::string U = textio::up(cmd);

        if (U == "QUIT" || U == "EXIT") break;

        DbArea& cur = eng.area(eng.currentArea());
        if (!cli::registry().run(cur, U, iss)) {   // <-- correct order: (DbArea&, cmd, stream)
            std::cout << "Unknown command: " << cmd << "\n";
        }
    }
    return 0;
}

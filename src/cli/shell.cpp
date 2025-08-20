// shell.cpp — restored command registry + indexing hooks
#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <array>   // <-- added
#include "xbase.hpp"
#include "textio.hpp"
#include "command_registry.hpp"
#include "colors.hpp"
#include "cmd_version.hpp"

using xbase::DbArea;

// ---- (NEW) simple column printer + curated lists for HELP BROKEN/STUBBED ----
namespace {
    template <typename Seq>
    void print_cmd_list(const Seq& seq) {
        if (seq.size() == 0) {
            std::cout << "  (none)\n";
            return;
        }
        int col = 0;
        for (const auto& s : seq) {
            std::cout << "  " << s;
            ++col;
            if (col == 6) { std::cout << "\n"; col = 0; }
        }
        if (col != 0) std::cout << "\n";
    }

    // Tweak these lists as your implementation status changes:
    static constexpr std::array<const char*, 0> HELP_BROKEN_CMDS{};          // ok: zero-length std::array
    static constexpr std::array<const char*, 2> HELP_STUBBED_CMDS{{"LOCATE", "ZAP"}};
}

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
void cmd_FIND (xbase::DbArea&, std::istringstream&);
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
void cmd_APPEND_BLANK(xbase::DbArea&, std::istringstream&);
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

    // --- Engine/area helpers ---
    cli::reg.add("USE",     [](DbArea& A, std::istringstream& S){ cmd_USE(A,S); });

    cli::reg.add("SELECT",  [&](DbArea& /*A*/, std::istringstream& S){
        int n; S >> n;
        if (!S || n < 0 || n >= MAX_AREA) {
            std::cout << "Usage: SELECT <0.." << (MAX_AREA-1) << ">\n";
            return;
        }
        eng.selectArea(n);
        std::cout << "Selected area " << n << ".\n";
    });

    cli::reg.add("AREA",    [&](DbArea&, std::istringstream&){
        int i = eng.currentArea();
        DbArea& cur = eng.area(i);
        std::cout << "Current area: " << i << "\n";
        if (cur.isOpen())
            std::cout << "  File: " << cur.name()
                      << "  Recs: " << cur.recCount()
                      << "  Recno: " << cur.recno() << "\n";
        else
            std::cout << "  (no file open)\n";
    });

    // --- Record navigation & reporting ---
    cli::reg.add("LIST",    [](DbArea& A, std::istringstream& S){ cmd_LIST(A,S); });
    cli::reg.add("TOP",     [](DbArea& A, std::istringstream& S){ cmd_TOP(A,S); });
    cli::reg.add("BOTTOM",  [](DbArea& A, std::istringstream& S){ cmd_BOTTOM(A,S); });
    cli::reg.add("GOTO",    [](DbArea& A, std::istringstream& S){ cmd_GOTO(A,S); });
    cli::reg.add("COUNT",   [](DbArea& A, std::istringstream& S){ cmd_COUNT(A,S); });
    cli::reg.add("DISPLAY", [](DbArea& A, std::istringstream& S){ cmd_DISPLAY(A,S); });

    // --- Data I/O & maintenance ---
    cli::reg.add("COPY",    [](DbArea& A, std::istringstream& S){ cmd_COPY(A,S); });
    cli::reg.add("EXPORT",  [](DbArea& A, std::istringstream& S){ cmd_EXPORT(A,S); });
    cli::reg.add("IMPORT",  [](DbArea& A, std::istringstream& S){ cmd_IMPORT(A,S); });
    cli::reg.add("APPEND",  [](DbArea& A, std::istringstream& S){ cmd_APPEND(A,S); });
    cli::reg.add("DELETE",  [](DbArea& A, std::istringstream& S){ cmd_DELETE(A,S); });
    cli::reg.add("RECALL",  [](DbArea& A, std::istringstream& S){ cmd_RECALL(A,S); });
    cli::reg.add("UNDELETE",[](DbArea& A, std::istringstream& S){ cmd_RECALL(A,S); });
    cli::reg.add("PACK",    [](DbArea& A, std::istringstream& S){ cmd_PACK(A,S); });
    cli::reg.add("FIELDS",  [](DbArea& A, std::istringstream& S){ cmd_FIELDS(A,S); });

    // --- Search ---
    cli::reg.add("FIND",    [](DbArea& A, std::istringstream& S){ cmd_FIND(A,S); });
    cli::reg.add("SEEK",    [](DbArea& A, std::istringstream& S){ cmd_SEEK(A,S); });

    // --- UI/utility ---
    cli::reg.add("COLOR",   [](DbArea& A, std::istringstream& S){ cmd_COLOR(A,S); });
    cli::reg.add("VERSION", [](DbArea& A, std::istringstream& S){ cmd_VERSION(A,S); });

    // --- Indexing (only when enabled) ---
#if DOTTALK_WITH_INDEX
    cli::reg.add("INDEX",     [](DbArea& A, std::istringstream& S){ cmd_INDEX(A,S); });
    cli::reg.add("SETINDEX",  [](DbArea& A, std::istringstream& S){ cmd_SETINDEX(A,S); });
    cli::reg.add("SET INDEX", [](DbArea& A, std::istringstream& S){ cmd_SETINDEX(A,S); }); // if spaced keys are supported
    cli::reg.add("SET INDEX TO", [](DbArea& A, std::istringstream& S){ cmd_SETINDEX(A,S); }); // optional nicety
    cli::reg.add("ASCEND",    [](DbArea& A, std::istringstream& S){ cmd_ASCEND(A,S); });
    cli::reg.add("DESCEND",   [](DbArea& A, std::istringstream& S){ cmd_DESCEND(A,S); });
    cli::reg.add("SETORDER",  [](DbArea& A, std::istringstream& S){ cmd_SETORDER(A,S); });
    cli::reg.add("SET ORDER", [](DbArea& A, std::istringstream& S){ cmd_SETORDER(A,S); });
#endif

    cli::reg.add("APPEND BLANK", [](DbArea& A, std::istringstream& S){ cmd_APPEND_BLANK(A,S); });
    cli::reg.add("CLEAR",        [](DbArea& A, std::istringstream& S){ cmd_CLEAR(A,S); });
    cli::reg.add("CREATE",       [](DbArea& A, std::istringstream& S){ cmd_CREATE(A,S); });
    cli::reg.add("DUMP",         [](DbArea& A, std::istringstream& S){ cmd_DUMP(A,S); });
    cli::reg.add("EDIT",         [](DbArea& A, std::istringstream& S){ cmd_EDIT(A,S); });
//  cli::reg.add("LOCATE",       [](DbArea& A, std::istringstream& S){ cmd_LOCATE(A,S); });
    cli::reg.add("RECNO",        [](DbArea& A, std::istringstream& S){ cmd_RECNO(A,S); });
    cli::reg.add("REFRESH",      [](DbArea& A, std::istringstream& S){ cmd_REFRESH(A,S); });
    cli::reg.add("REPLACE",      [](DbArea& A, std::istringstream& S){ cmd_REPLACE(A,S); });
    cli::reg.add("STATUS",       [](DbArea& A, std::istringstream& S){ cmd_STATUS(A,S); });
    cli::reg.add("STRUCT",       [](DbArea& A, std::istringstream& S){ cmd_STRUCT(A,S); });
//  cli::reg.add("ZAP",          [](DbArea& A, std::istringstream& S){ cmd_ZAP(A,S); });

// System / utilities
    cli::reg.add("DIR",     [](DbArea& A, std::istringstream& S){ cmd_DIR(A,S);   });
    cli::reg.add("!",       [](DbArea& A, std::istringstream& S){ cmd_BANG(A,S);  });
//  cli::reg.add("RECNO",   [](DbArea& A, std::istringstream& S){ cmd_RECNO(A,S); });
    cli::reg.add("CLS",          cmd_CLEAR);

 // cli::reg.add("SETINDEX",     [](DbArea& A, std::istringstream& S){ cmd_SETINDEX(A,S); });
 // cli::reg.add("SET INDEX",    [](DbArea& A, std::istringstream& S){ cmd_SETINDEX(A,S); });
 // cli::reg.add("SET INDEX TO", [](DbArea& A, std::istringstream& S){ cmd_SETINDEX(A,S); }); // optional nicety
 // cli::reg.add("SETORDER",     [](DbArea& A, std::istringstream& S){ cmd_SETORDER(A,S); });

// Optional helpful aliases:
    cli::reg.add("CLS",          cmd_CLEAR);

    // ---- HELP with optional BROKEN/STUBBED argument ----
    cli::reg.add("HELP", [&](DbArea&, std::istringstream& S){
        std::string arg; S >> arg;
        arg = textio::up(textio::trim(arg));
        if (arg.empty()) {
            cli::reg.help(std::cout);
            return;
        }
        if (arg == "BROKEN") {
            std::cout << "BROKEN commands (work to do):\n";
            print_cmd_list(HELP_BROKEN_CMDS);
            return;
        }
        if (arg == "STUBBED") {
            std::cout << "STUBBED commands (planned/not implemented):\n";
            print_cmd_list(HELP_STUBBED_CMDS);
            return;
        }
        std::cout << "Usage: HELP [BROKEN|STUBBED]\n";
    });

    std::cout << "DotTalk++ type HELP. USE, SELECT <n>, AREA, COLOR <GREEN|AMBER|DEFAULT>, QUIT.\n";

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
        if (!cli::reg.run(cur, U, iss)) {   // <-- correct order: (DbArea&, cmd, stream)
            std::cout << "Unknown command: " << cmd << "\n";
        }
    }
    return 0;
}

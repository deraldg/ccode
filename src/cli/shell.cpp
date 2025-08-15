#include <iostream>
#include <sstream>
#include <string>
#include "xbase.hpp"
#include "textio.hpp"
#include "command_registry.hpp"
#include "colors.hpp"

using xbase::DbArea;

// forward decl matches definition exactly
void cmd_VERSION(DbArea&, std::istringstream&);


// Command handlers declared elsewhere
void cmd_USE(xbase::DbArea&, std::istringstream&);
void cmd_LIST(xbase::DbArea&, std::istringstream&);
void cmd_COPY(xbase::DbArea&, std::istringstream&);
void cmd_EXPORT(xbase::DbArea&, std::istringstream&);
void cmd_IMPORT(xbase::DbArea&, std::istringstream&);
void cmd_APPEND(xbase::DbArea&, std::istringstream&);

void cmd_TOP(xbase::DbArea&, std::istringstream&);
void cmd_BOTTOM(xbase::DbArea&, std::istringstream&);
void cmd_GOTO(xbase::DbArea&, std::istringstream&);
void cmd_COUNT(xbase::DbArea&, std::istringstream&);
void cmd_DISPLAY(xbase::DbArea&, std::istringstream&);
void cmd_DELETE(xbase::DbArea&, std::istringstream&);
void cmd_RECALL(xbase::DbArea&, std::istringstream&);
void cmd_PACK(xbase::DbArea&, std::istringstream&);
void cmd_COLOR(xbase::DbArea&, std::istringstream&);

void cmd_SEEK(xbase::DbArea&, std::istringstream&);
void cmd_FIND(xbase::DbArea&, std::istringstream&);


#if DOTTALK_WITH_INDEX
void cmd_SEEK(xbase::DbArea&, std::istringstream&);
#endif

int run_shell()
{
    using namespace xbase;
    using namespace cli;

    // Apply default theme (GREEN on black). Change with COLOR AMBER or COLOR DEFAULT.
    colors::applyTheme(colors::Theme::Green);
    struct ResetAtExit { ~ResetAtExit(){ colors::reset(); } } _reset_guard;

    XBaseEngine eng;
    eng.selectArea(0);

    CommandRegistry reg;
    reg.add("USE",     [](DbArea& A, std::istringstream& S){ cmd_USE(A,S); });
    reg.add("SELECT",  [&](DbArea& A, std::istringstream& S){
        (void)A;
        int n; S >> n;
        if (!S || n < 0 || n >= MAX_AREA) {
            std::cout << "Usage: SELECT <0.." << (MAX_AREA-1) << ">" << std::endl;
            return;
        }
        eng.selectArea(n);
        std::cout << "Selected area " << n << "." << std::endl;
    });
    reg.add("AREA",    [&](DbArea&, std::istringstream&){
        int i = eng.currentArea();
        DbArea& cur = eng.area(i);
        std::cout << "Current area: " << i << std::endl;
        if (cur.isOpen())
            std::cout << "  File: " << cur.name() << "  Recs: " << cur.recCount()
                      << "  Recno: " << cur.recno() << std::endl;
        else
            std::cout << "  (no file open)" << std::endl;
    });
    reg.add("LIST",    [](DbArea& A, std::istringstream& S){ cmd_LIST(A,S); });
    reg.add("COPY",    [](DbArea& A, std::istringstream& S){ cmd_COPY(A,S); });
    reg.add("EXPORT",  [](DbArea& A, std::istringstream& S){ cmd_EXPORT(A,S); });
    reg.add("IMPORT",  [](DbArea& A, std::istringstream& S){ cmd_IMPORT(A,S); });
    reg.add("APPEND",  [](DbArea& A, std::istringstream& S){ cmd_APPEND(A,S); });

    reg.add("TOP",     [](DbArea& A, std::istringstream& S){ cmd_TOP(A,S); });
    reg.add("BOTTOM",  [](DbArea& A, std::istringstream& S){ cmd_BOTTOM(A,S); });
    reg.add("GOTO",    [](DbArea& A, std::istringstream& S){ cmd_GOTO(A,S); });
    reg.add("COUNT",   [](DbArea& A, std::istringstream& S){ cmd_COUNT(A,S); });
    reg.add("DISPLAY", [](DbArea& A, std::istringstream& S){ cmd_DISPLAY(A,S); });
    reg.add("DELETE",  [](DbArea& A, std::istringstream& S){ cmd_DELETE(A,S); });
    reg.add("RECALL",  [](DbArea& A, std::istringstream& S){ cmd_RECALL(A,S); });
    reg.add("UNDELETE",[](DbArea& A, std::istringstream& S){ cmd_RECALL(A,S); });
    reg.add("PACK",    [](DbArea& A, std::istringstream& S){ cmd_PACK(A,S); });
    reg.add("COLOR",   [](DbArea& A, std::istringstream& S){ cmd_COLOR(A,S); });
    reg.add("SEEK",    [](DbArea& A, std::istringstream& S){ cmd_SEEK(A, S); });
    reg.add("FIND",    [](DbArea& A, std::istringstream& S){ cmd_FIND(A, S); });
    reg.add("VERSION", [](DbArea& A, std::istringstream& S){ cmd_VERSION(A, S); });


    reg.add("VERSION", [](xbase::DbArea& A, std::istringstream& S){ cmd_VERSION(A, S); });


#if DOTTALK_WITH_INDEX
    reg.add("SEEK",    [](DbArea& A, std::istringstream& S){ cmd_SEEK(A,S); });
#endif

    reg.add("HELP",    [&](DbArea&, std::istringstream&){ reg.help(std::cout); });

    std::cout << "DotTalk++ - type HELP. SELECT <n>, AREA, COLOR <GREEN|AMBER|DEFAULT>, QUIT." << std::endl;

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
        if (!reg.run(U, cur, iss)) {
            std::cout << "Unknown command: " << cmd << std::endl;
        }
    }
    return 0;
}

// src/cli/cmd_set.cpp
// FoxPro-style SET command router for DotTalk++
//
// Policy:
// - Public surface is "SET <option> ..."
// - Keep grammar massaging thin here.
// - Let target commands own detailed parsing/validation.
// - Hide developer / transitional options unless DOTTALK_WITH_DEV is enabled.

#include "xbase.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#include "cli/settings.hpp"
#include "cli/output_router.hpp"

// ---- Forward declarations ---------------------------------------------------
void cmd_SETINDEX      (xbase::DbArea&, std::istringstream&);
void cmd_SETORDER      (xbase::DbArea&, std::istringstream&);
void cmd_SETFILTER     (xbase::DbArea&, std::istringstream&);
void cmd_SET_UNIQUE    (xbase::DbArea&, std::istringstream&);
void cmd_SET_RELATION  (xbase::DbArea&, std::istringstream&);
void cmd_SET_RELATIONS (xbase::DbArea&, std::istringstream&);
void cmd_SETCASE       (xbase::DbArea&, std::istringstream&);
void cmd_SETPATH       (xbase::DbArea&, std::istringstream&);
void cmd_SETTIMER      (xbase::DbArea&, std::istringstream&);

#if DOTTALK_WITH_DEV
void cmd_SETCNX        (xbase::DbArea&, std::istringstream&);
void cmd_SETCDX        (xbase::DbArea&, std::istringstream&);
void cmd_SETLMDB       (xbase::DbArea&, std::istringstream&);
#endif

namespace {

static inline std::string up_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

static inline std::string ltrim_copy(std::string s) {
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    return s.substr(i);
}

static inline std::string rest(std::istringstream& iss) {
    return std::string(std::istreambuf_iterator<char>(iss),
                       std::istreambuf_iterator<char>());
}

static inline bool parse_on_off(const std::string& tok, bool& out) {
    const std::string u = up_copy(tok);
    if (u == "ON")  { out = true;  return true; }
    if (u == "OFF") { out = false; return true; }
    return false;
}

static void print_set_usage() {
    auto& out = cli::OutputRouter::instance().out();

    out
        << "Usage: SET <option> [args]\n"
        << "Public options:\n"
        << "  SET CONSOLE ON|OFF\n"
        << "  SET PRINT ON|OFF\n"
        << "  SET PRINT TO <file>\n"
        << "  SET PRINT TO              (close)\n"
        << "  SET ALTERNATE ON|OFF\n"
        << "  SET ALTERNATE TO <file>\n"
        << "  SET ALTERNATE TO          (close)\n"
        << "  SET TALK ON|OFF\n"
        << "  SET ECHO ON|OFF\n"
        << "  SET PAGING ON|OFF\n"
        << "  SET DELETED ON|OFF\n"
        << "  SET PATH <slot> <path>\n"
        << "  SET TIMER ON|OFF\n"
        << "  SET POLLING ON|OFF\n"
        << "  SET INDEX TO <file>\n"
        << "  SET ORDER TO <tag|0>\n"
        << "  SET ORDER TO TAG <tag>\n"
        << "  SET ORDER TO TAG <tag> IN <alias>\n"
        << "  SET UNIQUE <args...>\n"
        << "  SET CASE <args...>\n";

#if DOTTALK_WITH_DEV
    out
        << "\n"
        << "Developer / transitional:\n"
        << "  SET FILTER TO <expr>\n"
        << "  SET RELATION <args...>\n"
        << "  SET RELATIONS <args...>\n"
        << "  SET CNX [TO] <container.cnx>\n"
        << "  SET CDX [TO] <container.cdx>\n"
        << "  SET LMDB <args...>\n";
#endif
}

static std::string pass_through_optional_to(std::istringstream& args) {
    std::string first;
    args >> first;
    std::string tail = ltrim_copy(rest(args));
    std::string pass;

    if (!first.empty() && up_copy(first) != "TO") {
        pass = first;
        if (!tail.empty()) pass += " " + tail;
    } else {
        pass = tail;
    }
    return pass;
}

} // namespace

void cmd_SET(xbase::DbArea& A, std::istringstream& args) {
    using cli::Settings;

    auto& S   = Settings::instance();
    auto& R   = cli::OutputRouter::instance();
    auto& out = R.out();

    std::string opt;
    if (!(args >> opt)) {
        print_set_usage();
        return;
    }
    opt = up_copy(opt);

    if (opt == "CONSOLE") {
        std::string tok; args >> tok;
        bool on = R.console_on();
        if (!parse_on_off(tok, on)) {
            out << "Usage: SET CONSOLE ON|OFF\n";
            return;
        }
        R.set_console(on);
        out << "Console is " << (on ? "ON" : "OFF") << "\n";
        return;
    }

    if (opt == "PRINT") {
        std::string tok; args >> tok;
        if (tok.empty()) {
            out << "Usage: SET PRINT ON|OFF | SET PRINT TO <file>\n";
            return;
        }

        const std::string u = up_copy(tok);

        if (u == "TO") {
            std::string tail = ltrim_copy(rest(args));
            if (tail.empty()) {
                R.close_print_to();
                out << "PRINT TO closed.\n";
                return;
            }

            if (!R.set_print_to(tail)) {
                out << "PRINT TO failed: " << tail << "\n";
                return;
            }
            out << "PRINT TO: " << R.print_to_path() << "\n";
            return;
        }

        bool on = R.print_on();
        if (!parse_on_off(tok, on)) {
            out << "Usage: SET PRINT ON|OFF | SET PRINT TO <file>\n";
            return;
        }
        R.set_print(on);
        out << "Print is " << (on ? "ON" : "OFF") << "\n";
        return;
    }

    if (opt == "ALTERNATE") {
        std::string tok; args >> tok;
        if (tok.empty()) {
            out << "Usage: SET ALTERNATE ON|OFF | SET ALTERNATE TO <file>\n";
            return;
        }

        const std::string u = up_copy(tok);

        if (u == "TO") {
            std::string tail = ltrim_copy(rest(args));
            if (tail.empty()) {
                R.close_alternate_to();
                out << "ALTERNATE TO closed.\n";
                return;
            }

            if (!R.set_alternate_to(tail)) {
                out << "ALTERNATE TO failed: " << tail << "\n";
                return;
            }
            out << "ALTERNATE TO: " << R.alternate_to_path() << "\n";
            return;
        }

        bool on = R.alternate_on();
        if (!parse_on_off(tok, on)) {
            out << "Usage: SET ALTERNATE ON|OFF | SET ALTERNATE TO <file>\n";
            return;
        }
        R.set_alternate(on);
        out << "Alternate is " << (on ? "ON" : "OFF") << "\n";
        return;
    }

    if (opt == "TALK") {
        std::string tok; args >> tok;
        bool on = S.talk_on.load();
        if (!parse_on_off(tok, on)) {
            out << "Usage: SET TALK ON|OFF\n";
            return;
        }
        S.talk_on.store(on);
        out << "Talk is " << (on ? "ON" : "OFF") << "\n";
        return;
    }

    if (opt == "TIMER") {
        std::string tok; args >> tok;
        bool on = S.timer_on.load();
        if (!parse_on_off(tok, on)) {
            out << "Usage: SET TIMER ON|OFF\n";
            return;
        }
        S.timer_on.store(on);
        out << "Timer is " << (on ? "ON" : "OFF") << "\n";
        return;
    }

    if (opt == "POLLING") {
        std::string tok; args >> tok;
        bool on = S.polling_on.load();
        if (!parse_on_off(tok, on)) {
            out << "Usage: SET POLLING ON|OFF\n";
            return;
        }
        S.polling_on.store(on);
        out << "Polling is " << (on ? "ON" : "OFF") << "\n";
        return;
    }

    if (opt == "ECHO") {
        std::string tok; args >> tok;
        bool on = R.echo_on();
        if (!parse_on_off(tok, on)) {
            out << "Usage: SET ECHO ON|OFF\n";
            return;
        }
        R.set_echo(on);
        out << "Echo is " << (on ? "ON" : "OFF") << "\n";
        return;
    }

    if (opt == "PAGING") {
        std::string tok; args >> tok;
        bool on = R.paging_on();
        if (!parse_on_off(tok, on)) {
            out << "Usage: SET PAGING ON|OFF\n";
            return;
        }
        R.set_paging(on);
        out << "Paging is " << (on ? "ON" : "OFF") << "\n";
        return;
    }

    if (opt == "DELETED") {
        std::string tok; args >> tok;
        bool on = S.deleted_on.load();
        if (!parse_on_off(tok, on)) {
            out << "Usage: SET DELETED ON|OFF\n";
            return;
        }
        S.deleted_on.store(on);
        out << "Deleted visibility: " << (on ? "HIDE (ON)" : "SHOW (OFF)") << "\n";
        return;
    }

    if (opt == "PATH") {
        std::istringstream r(rest(args));
        cmd_SETPATH(A, r);
        return;
    }

    if (opt == "INDEX") {
        std::istringstream r(pass_through_optional_to(args));
        cmd_SETINDEX(A, r);
        return;
    }

    if (opt == "ORDER") {
        std::istringstream r(pass_through_optional_to(args));
        cmd_SETORDER(A, r);
        return;
    }

    if (opt == "UNIQUE") {
        std::istringstream r(rest(args));
        cmd_SET_UNIQUE(A, r);
        return;
    }

    if (opt == "CASE") {
        std::istringstream r(rest(args));
        cmd_SETCASE(A, r);
        return;
    }

#if DOTTALK_WITH_DEV
    if (opt == "FILTER") {
        std::string first; args >> first;
        std::string tail = ltrim_copy(rest(args));
        std::string pass;

        if (!first.empty() && up_copy(first) != "TO") {
            pass = "TO " + first;
            if (!tail.empty()) pass += " " + tail;
        } else if (up_copy(first) == "TO") {
            pass = "TO";
            if (!tail.empty()) pass += " " + tail;
        } else {
            pass = "TO";
        }

        std::istringstream r(pass);
        cmd_SETFILTER(A, r);
        return;
    }

    if (opt == "RELATION") {
        std::istringstream r(rest(args));
        cmd_SET_RELATION(A, r);
        return;
    }

    if (opt == "RELATIONS") {
        std::istringstream r(rest(args));
        cmd_SET_RELATIONS(A, r);
        return;
    }

    if (opt == "CNX") {
        std::istringstream r(pass_through_optional_to(args));
        cmd_SETCNX(A, r);
        return;
    }

    if (opt == "CDX") {
        std::istringstream r(pass_through_optional_to(args));
        cmd_SETCDX(A, r);
        return;
    }

    if (opt == "LMDB") {
        std::istringstream r(rest(args));
        cmd_SETLMDB(A, r);
        return;
    }
#endif

    print_set_usage();
}

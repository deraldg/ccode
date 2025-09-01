// cmd_browse.cpp
// DotTalk++ — Minimal interactive BROWSE with editing, no DbArea internals.
// Uses existing handlers: LIST, DISPLAY, GOTO, REPLACE.

#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>

#include "xbase.hpp"  // xbase::DbArea

// ---- Forward declarations of existing command handlers ---------------------
void cmd_LIST   (xbase::DbArea&, std::istringstream&);
void cmd_DISPLAY(xbase::DbArea&, std::istringstream&);
void cmd_GOTO   (xbase::DbArea&, std::istringstream&);
void cmd_REPLACE(xbase::DbArea&, std::istringstream&);

// ---- Helpers ---------------------------------------------------------------
static inline std::string trim(const std::string& s) {
  size_t b = 0, e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e-1]))) --e;
  return s.substr(b, e - b);
}

static inline std::string upper(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
  return s;
}

static inline bool needs_quotes(const std::string& v) {
  for (unsigned char c : v) {
    if (std::isspace(c) || !std::isalnum(c)) return true;
  }
  return v.empty();
}

static void show_menu(bool editEnabled) {
  std::cout
    << "\n=== BROWSE " << (editEnabled ? "EDIT " : "") << "===\n"
    << "Commands:\n"
    << "  L            - List (uses LIST)\n"
    << "  D            - Display current record (uses DISPLAY)\n"
    << "  G <n>        - Go to record n (uses GOTO)\n"
    << "  E            - Edit a field in current record (uses REPLACE)\n"
    << "  H or ?       - Help (this menu)\n"
    << "  Q            - Quit browse\n";
}

// ---- Command implementation ------------------------------------------------
void cmd_BROWSE(xbase::DbArea& area, std::istringstream& iss)
{
  // Parse optional "EDIT" token, but we allow E command regardless.
  bool editMode = false;
  {
    std::string rest; std::getline(iss, rest);
    rest = upper(trim(rest));
    if (rest.find("EDIT") != std::string::npos) editMode = true;
  }

  std::cout << "Entered BROWSE" << (editMode ? " EDIT" : "") << " mode.\n";
  show_menu(editMode);

  for (;;) {
    std::cout << "\nBROWSE> ";
    std::string line;
    if (!std::getline(std::cin, line)) break;
    line = trim(line);
    if (line.empty()) continue;

    // Split first token
    std::istringstream ls(line);
    std::string cmd; ls >> cmd;
    std::string arg; std::getline(ls, arg); arg = trim(arg);

    std::string ucmd = upper(cmd);

    // Quit
    if (ucmd == "Q" || ucmd == "QUIT" || ucmd == "EXIT") {
      std::cout << "Leaving BROWSE.\n";
      break;
    }

    // Help
    if (ucmd == "H" || ucmd == "?") {
      show_menu(editMode);
      continue;
    }

    // List (delegate to LIST)
    if (ucmd == "L" || ucmd == "LIST") {
      std::istringstream empty;
      cmd_LIST(area, empty);
      continue;
    }

    // Display current record (delegate to DISPLAY)
    if (ucmd == "D" || ucmd == "DISPLAY") {
      std::istringstream empty;
      cmd_DISPLAY(area, empty);
      continue;
    }

    // GOTO <n>
    if (ucmd == "G" || ucmd == "GOTO") {
      if (arg.empty()) {
        std::cout << "Usage: G <recno>\n";
        continue;
      }
      std::istringstream go(arg);
      cmd_GOTO(area, go);
      // show the record after moving
      std::istringstream empty;
      cmd_DISPLAY(area, empty);
      continue;
    }

    // Edit current record: prompt for field and value, call REPLACE
    if (ucmd == "E" || ucmd == "EDIT") {
      // We allow edit regardless of initial EDIT token (keeps it simple)
      std::string field;
      std::cout << "Field name: ";
      if (!std::getline(std::cin, field)) break;
      field = trim(field);
      if (field.empty()) {
        std::cout << "Canceled (empty field).\n";
        continue;
      }

      std::string newv;
      std::cout << "New value: ";
      if (!std::getline(std::cin, newv)) break;
      newv = trim(newv);

      // Build REPLACE <FIELD> WITH <value>
      std::ostringstream ro;
      ro << field << " WITH ";
      if (needs_quotes(newv)) ro << '"' << newv << '"';
      else                     ro << newv;

      std::istringstream riss(ro.str());
      cmd_REPLACE(area, riss);

      // Show the updated record
      std::istringstream empty;
      cmd_DISPLAY(area, empty);
      continue;
    }

    std::cout << "Unknown command. Type H for help.\n";
  }
}

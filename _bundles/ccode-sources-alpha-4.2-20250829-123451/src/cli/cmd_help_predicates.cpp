#include <iostream>
#include <sstream>
#include <string>
#include "command_registry.hpp"
#include "xbase.hpp"

static void print_predhelp() {
    std::cout <<
R"(Predicate help (COUNT/LOCATE)

Syntax:
  COUNT [ALL|DELETED] [FOR <field> <op> <value>]
  LOCATE [FOR <field> <op> <value>]

Operators:
  Strings:   =  !=  $ (substring)
  Numbers:   =  !=  >  >=  <  <=
  Dates:     use YYYYMMDD (e.g., DOB >= 20000101)
  Booleans:  =  !=  (T/F, .T./.F., TRUE/FALSE, 1/0)

Case:
  SETCASE OFF  (default) => string compares are case-insensitive
  SETCASE ON             => string compares are case-sensitive

Functions (left side):
  UPPER(field) / LOWER(field)
    e.g., COUNT FOR UPPER(LAST_NAME) $ "MIT"
          COUNT FOR LOWER(LAST_NAME) = "smith"

Inline comments:
  Use && to comment the rest of the line.

Examples:
  COUNT FOR LAST_NAME = "Smith"
  COUNT FOR LAST_NAME $ "MIT"
  COUNT FOR GPA >= 3
  COUNT FOR DOB >= 20000101
  COUNT FOR IS_ACTIVE = .T.
  LOCATE FOR UPPER(LAST_NAME) = "SMITH"
)";
}

void cmd_PREDHELP(xbase::DbArea&, std::istringstream&)     { print_predhelp(); }
void cmd_PREDICATES(xbase::DbArea&, std::istringstream&)   { print_predhelp(); }

static bool s_reg = [](){
    cli::registry().add("PREDHELP",     &cmd_PREDHELP);
    cli::registry().add("PREDICATES",   &cmd_PREDICATES);
    return true;
}();

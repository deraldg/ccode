// ============================================================================
// path: src/cli/cmd_loop.hpp
// purpose: LOOP command declarations + executor hook
// ============================================================================
#pragma once
#include <sstream>
#include <string>

namespace xbase { class DbArea; }

// pluggable executor for replaying buffered lines
using LoopExecFn = void(*)(xbase::DbArea&, const std::string&);

// call once from shell to provide a dispatcher
void loop_set_executor(LoopExecFn fn);

// optional: read back the executor for WHILE/UNTIL reuse
LoopExecFn loop_get_executor();

// core LOOP commands
void cmd_LOOP        (xbase::DbArea& A, std::istringstream& S);
void cmd_LOOP_BUFFER (xbase::DbArea& A, std::istringstream& S);
void cmd_ENDLOOP     (xbase::DbArea& A, std::istringstream& S);

// WHILE/UNTIL hooks (shell wires these to its boolean evaluator)
extern "C" void while_set_condition_eval(bool(*)(xbase::DbArea&, const std::string&));
extern "C" void until_set_condition_eval(bool(*)(xbase::DbArea&, const std::string&));

extern "C" bool while_is_active();
void cmd_WHILE_BUFFER(xbase::DbArea& A, std::istringstream& S);

// WHILE/UNTIL commands
void cmd_WHILE    (xbase::DbArea& A, std::istringstream& S);
void cmd_ENDWHILE (xbase::DbArea& A, std::istringstream& S);
void cmd_UNTIL    (xbase::DbArea& A, std::istringstream& S);
void cmd_ENDUNTIL (xbase::DbArea& A, std::istringstream& S);




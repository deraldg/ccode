#include <iostream>
#include <sstream>
#include "runtime/utf8_init.hpp"


int run_shell(); // already implemented elsewhere

int main(int argc, char** argv) {
    (void)argc; (void)argv;
  dottalk::init_utf8();

  try { return run_shell(); }
  catch (const std::exception& ex) {
    std::cerr << "Fatal error: " << ex.what() << "\n";
    return 1;
  }
}

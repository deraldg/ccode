#include <iostream>
int run_shell(); // implemented in shell.cpp

int main() {
    try { return run_shell(); }
    catch (const std::exception& ex) { std::cerr << "Fatal error: " << ex.what() << "\n"; return 1; }
}

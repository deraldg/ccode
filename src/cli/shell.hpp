// src/cli/shell.hpp
#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include "xbase.hpp"
#include "command_registry.hpp"

class Shell {
public:
    Shell();
    ~Shell();

    // Basic text I/O helpers
    void print(const std::string& s) { std::cout << s; }
    void println(const std::string& s) { std::cout << s << std::endl; }

    // Access to the database API (placeholder until we wire full shell_api)
    xbase::DbArea* current_area() { return _area; }
    const xbase::DbArea* current_area() const { return _area; }

    // placeholder API adaptor; in real builds, this is more complex
    struct ApiProxy {
        xbase::DbArea* current_area() { return nullptr; }
    };
    ApiProxy& api() { static ApiProxy proxy; return proxy; }

private:
    xbase::DbArea* _area = nullptr;
};

// Forward declaration for run_shell, defined in shell.cpp
int run_shell(bool quiet_mode);

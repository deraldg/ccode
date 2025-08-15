// cmd_version.cpp — VERSION command: shows build date/time
#include <iostream>
#include <sstream>
#include <string>
#include "xbase.hpp"

using xbase::DbArea;

constexpr const char* build_date = __DATE__;
constexpr const char* build_time = __TIME__;

void cmd_VERSION(DbArea&, std::istringstream&) {
    std::cout << "DotTalk++ build: " << build_date << " " << build_time << "\n";
}

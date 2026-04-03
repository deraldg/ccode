// src/cli/cmd_about.cpp
//
// ABOUT
// Print project identity, lineage, author, history, and runtime environment.

#include "cli/cmd_about.hpp"
#include "xbase/about_info.hpp"
#include "xbase.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__WINDOWS__)
    #define DT_OS_FAMILY "Windows"
    #ifdef _WIN64
        #define DT_ARCH "x64"
    #else
        #define DT_ARCH "x86"
    #endif
#elif defined(__linux__) || defined(__linux) || defined(linux)
    #define DT_OS_FAMILY "Linux"
    #ifdef __x86_64__
        #define DT_ARCH "x86_64"
    #elif defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)
        #define DT_ARCH "x86"
    #elif defined(__aarch64__)
        #define DT_ARCH "ARM64"
    #else
        #define DT_ARCH "unknown"
    #endif
#elif defined(__APPLE__) || defined(__MACH__)
    #include <TargetConditionals.h>
    #define DT_OS_FAMILY "macOS"
    #if TARGET_CPU_ARM64
        #define DT_ARCH "Apple Silicon (arm64)"
    #else
        #define DT_ARCH "x86_64 (Intel)"
    #endif
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    #define DT_OS_FAMILY "FreeBSD"
    #define DT_ARCH "unknown"
#else
    #define DT_OS_FAMILY "Unknown platform"
    #define DT_ARCH "unknown"
#endif

namespace
{
    void print_section(const std::string& title)
    {
        std::cout << "\n" << title << "\n";
        std::cout << std::string(title.size(), '-') << "\n";
    }

    void print_kv(const std::string& key, const std::string& value)
    {
        std::cout << std::left << std::setw(16) << key << ": " << value << "\n";
    }

    std::string yes_no(bool v)
    {
        return v ? "Yes" : "No";
    }

    std::string safe_filename(const xbase::DbArea& db)
    {
        try
        {
            const std::string s = db.filename();
            return s.empty() ? "(none)" : s;
        }
        catch (...)
        {
            return "(none)";
        }
    }

    std::string safe_record_count(const xbase::DbArea& db)
    {
        try
        {
            std::ostringstream oss;
            oss << db.recCount();
            return oss.str();
        }
        catch (...)
        {
            return "(unavailable)";
        }
    }

    std::string safe_field_count(const xbase::DbArea& db)
    {
        try
        {
            std::ostringstream oss;
            oss << db.fields().size();
            return oss.str();
        }
        catch (...)
        {
            return "(unavailable)";
        }
    }

    std::string compiler_line()
    {
    #if defined(__GNUC__)
        std::ostringstream oss;
        oss << "GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__;
        return oss.str();
    #elif defined(_MSC_VER)
        std::ostringstream oss;
        oss << "MSVC " << _MSC_VER;
        return oss.str();
    #elif defined(__clang__)
        std::ostringstream oss;
        oss << "Clang " << __clang_major__ << "." << __clang_minor__ << "." << __clang_patchlevel__;
        return oss.str();
    #else
        return "Unknown";
    #endif
    }

    void print_page_1()
    {
        std::cout
            << "ABOUT - Page 1 of 2\n"
            << "===================\n"
            << "\n"
            << "DotTalk++\n"
            << "\n"
            << "Author\n"
            << " Derald Grimwood\n"
            << "\n"
            << "Dedicated to\n"
            << " Kathy Grimwood\n"
            << "\n"
            << "Project\n"
            << " DotTalk++ is a modern C++ xBase-inspired database runtime and command shell.\n"
            << "\n"
            << "Heritage\n"
            << " The DotTalk++ command model draws inspiration from the classic xBase\n"
            << " lineage of database languages:\n"
            << "\n"
            << " dBase - early interactive database shell\n"
            << " Clipper - compiled xBase systems\n"
            << " FoxPro - relational navigation and index-driven querying\n"
            << "\n"
            << " DotTalk++ preserves many of the familiar commands and workflows from\n"
            << " these systems while extending them with:\n"
            << "\n"
            << " modern help catalogs\n"
            << " relational traversal (REL ENUM)\n"
            << " tuple projection\n"
            << " scripting and automation\n"
            << "\n"
            << " DotTalk++ also combines aspects of both the interactive xBase model\n"
            << " and the compiled application model. Like dBase and FoxPro, it provides\n"
            << " a live command shell for exploring data. Like Clipper, it can be extended\n"
            << " through source code and compiled. It is also structured as modular\n"
            << " runtime libraries, including xbase and xindex, beneath the shell.\n"
            << "\n"
            << "History\n"
            << " The project traces back to 1993, when Derald Grimwood wrote a small\n"
            << " ANSI C database program as a practical and experimental system,\n"
            << " including fixed-length record storage and a simple in-memory B-tree.\n"
            << "\n"
            << " In 2025, that earlier work was revived and used as the conceptual basis\n"
            << " for a modern 64-bit rebuild in C++. The result became DotTalk++:\n"
            << " not a direct port, but a broader reimplementation and expansion.\n"
            << "\n"
            << "Current Direction\n"
            << " DotTalk++ aims to retain the clarity of the xBase interaction model\n"
            << " while making the engine suitable for modern experimentation and\n"
            << " education.\n"
            << "\n"
            << " It is intended to serve as:\n"
            << " - a working DBF database runtime\n"
            << " - a relational exploration environment\n"
            << " - a scripting and automation shell\n"
            << " - a teaching system for database concepts\n"
            << "\n"
            << " Internally, DotTalk++ is also organized as a modular system:\n"
            << " - xbase : core DBF/table/runtime library\n"
            << " - xindex : indexing library\n"
            << " - dottalk : command shell and interactive environment\n"
            << "\n"
            << " In this sense, DotTalk++ sits between FoxPro and Clipper:\n"
            << " - interactive and stateful like FoxPro\n"
            << " - extensible and compilable like Clipper\n"
            << "\n"
            << "Design Philosophy\n"
            << " DotTalk++ is intentionally stateful and interactive.\n"
            << "\n"
            << " It exposes important runtime concepts directly, including:\n"
            << " - current work area\n"
            << " - current record pointer\n"
            << " - active order/index\n"
            << " - active filter\n"
            << " - relation graph\n"
            << " - buffering state\n"
            << "\n"
            << " The goal is to make database behavior visible and understandable during\n"
            << " live operation, rather than hiding it behind abstraction.\n"
            << "\n"
            << "Working Model\n"
            << " DotTalk++ can be understood as four cooperating layers:\n"
            << "\n"
            << " 1. Command Layer - interactive commands and scripting\n"
            << " 2. Data Layer - tables, records, fields, indexes\n"
            << " 3. Logic Layer - expressions, predicates, control flow\n"
            << " 4. Projection Layer - LIST, SMARTLIST, TUPLE, REL ENUM, browsers\n"
            << "\n"
            << "Runtime Environment\n"
            << " OS family   : " << DT_OS_FAMILY << "\n"
            << " Architecture: " << DT_ARCH << "\n"
            << " C++ standard: C++" << (__cplusplus / 100) << "\n"
            << " Compiler    : " << compiler_line() << "\n"
            << "\n"
            << "Years\n"
            << " Origin: 1993 ANSI C\n"
            << " Revival / C++ X64 modern rebuild: 2025-\n"
            << "\n"
            << "Summary\n"
            << " DotTalk++ honors the xBase tradition while extending it into a modern,\n"
            << " teachable, experiment-friendly database runtime.\n" 
            << "\n"
            << " User interfaces change, languages change, but the underlying database principles remain constant. -- Derald Grimwood\n";
          
    }            

    void print_page_2(xbase::DbArea& db)
    {
        const std::string file_name = safe_filename(db);
        const std::string root = about_info::path_root_of(file_name);
        const about_info::DiskInfo dsk = about_info::disk_info(root);
        const about_info::ConsoleInfo con = about_info::console_size();

        std::cout
            << "\n"
            << "ABOUT - Page 2 of 2\n"
            << "===================\n";

        print_section("Application");
        print_kv("Name", about_info::app_name());
        print_kv("Build Mode", about_info::build_mode());
        print_kv("Build Date", about_info::build_date_time());
        print_kv("Architecture", about_info::architecture());
        print_kv("Compiler", about_info::compiler_string());
        print_kv("C++ Std", about_info::cpp_standard_string());

        print_section("Operating System");
        print_kv("OS", about_info::windows_version());

        print_section("Hardware");
        {
            std::ostringstream oss;
            oss << about_info::cpu_logical_count();
            print_kv("CPU Threads", oss.str());
        }
        print_kv("Installed RAM", about_info::format_bytes(about_info::installed_ram_bytes()));

        print_section("Storage");
        print_kv("Disk Root", dsk.root_path);
        print_kv("Disk Free", about_info::format_bytes(dsk.free_bytes));
        print_kv("Disk Total", about_info::format_bytes(dsk.total_bytes));

        print_section("Console");
        {
            std::ostringstream oss;
            oss << con.width << " x " << con.height;
            print_kv("Size", oss.str());
        }
        print_kv("ANSI / VT", about_info::vt_enabled() ? "enabled" : "disabled");

        print_section("Network");
        print_kv("Computer Name", about_info::computer_name_str());
        print_kv("Local IPv4", about_info::local_ipv4());

        print_section("Current Session");
        print_kv("File Open", yes_no(db.isOpen()));
        print_kv("Dbfile", file_name);
        print_kv("Records", safe_record_count(db));
        print_kv("Fields", safe_field_count(db));

        std::cout << "\n";
    }
}

void cmd_ABOUT(xbase::DbArea& db, std::istringstream&)
{
    print_page_1();
    print_page_2(db);
}
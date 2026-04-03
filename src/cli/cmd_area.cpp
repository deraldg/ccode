// src/cli/cmd_area.cpp
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>

#include "xbase.hpp"
#include "xbase/area_kind_util.hpp"
#include "cli/order_state.hpp"
#include "cli/order_report.hpp"
#include "workspace/workarea_utils.hpp"

using namespace xbase;
namespace fs = std::filesystem;

// Provided by shell.cpp
extern "C" XBaseEngine* shell_engine();

static int resolve_current_index(DbArea& A)
{
    XBaseEngine* eng = shell_engine();
    if (!eng) return -1;

    for (int i = 0; i < MAX_AREA; ++i) {
        if (&eng->area(i) == &A) return i;
    }
    return -1;
}

static std::string area_file_label(const DbArea& A)
{
    std::string s = A.logicalName();
    if (!s.empty()) return s;

    s = A.dbfBasename();
    if (!s.empty()) return s;

    const std::string& f = A.filename();
    if (!f.empty()) {
        fs::path p(f);
        auto stem = p.stem().string();
        if (!stem.empty()) return stem;
    }

    return "(unknown)";
}

void cmd_AREA(DbArea& A, std::istringstream&)
{
    const int idx = resolve_current_index(A);

    if (idx >= 0)
        std::cout << "Current area: " << idx
                  << " of " << workareas::occupied_desc() << "\n";
    else
        std::cout << "Current area: (unknown)\n";

    if (!A.isOpen()) {
        std::cout << "  (no file open in Area)\n";
        return;
    }

    const std::string label = area_file_label(A);

    std::cout << "  File: " << label
              << "  Recs: " << A.recCount()
              << "  Recno: " << A.recno() << "\n";

    const std::string& abs = A.filename();
    std::cout << "  DBF (abs)           : " << (abs.empty() ? "(unknown)" : abs) << "\n";
    std::cout << "  DBF Flavor          : " << xbase::area_kind_token(A.kind()) << "\n";

    const std::string ln = A.logicalName();
    std::cout << "  Logical name        : " << (ln.empty() ? "(unknown)" : ln) << "\n";

    const std::string legacy = A.name();
    std::cout << "  Legacy name()       : " << (legacy.empty() ? "(unknown)" : legacy) << "\n";

    if (!abs.empty()) {
        std::cout << "  Path: " << abs << "\n";
    }

    orderreport::print_area_one_line(std::cout, A);
}

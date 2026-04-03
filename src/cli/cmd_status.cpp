// src/cli/cmd_status.cpp

#include <iostream>
#include <iomanip>
#include <string>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <filesystem>

#include "xbase.hpp"
#include "xbase/area_kind_util.hpp"
#include "xindex/index_manager.hpp"
#include "workareas.hpp"
#include "workspace/workarea_utils.hpp"
#include "index_summary.hpp"
#include "cli/order_report.hpp"
#include "cli/path_resolver.hpp"

using dottalk::IndexSummary;

namespace {

std::string upper(std::string s) {
    std::transform(
        s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); }
    );
    return s;
}

bool is_open(const xbase::DbArea* a) {
    if (!a) return false;
    try {
        return a->isOpen() && !a->filename().empty();
    } catch (...) {
        return false;
    }
}

void print_area_header(std::size_t slot, const xbase::DbArea& A, bool isCurrent) {
    std::string base;
    try {
        base = std::filesystem::path(A.filename()).filename().string();
    } catch (...) {
        base = A.filename();
    }

    std::cout
        << "Area " << slot << ": " << A.filename()
        << "  (" << base << ")"
        << (isCurrent ? " [current]" : "")
        << "\n";
}

void print_workspace_line()
{
    std::cout << "Workspace : " << workareas::occupied_desc() << "\n";
}

void print_index_body(xbase::DbArea& A, bool verbose) {
    const IndexSummary S = dottalk::summarize_index(A);

    std::cout << "  DBF Flavor   : " << xbase::area_kind_token(A.kind()) << "\n";

    orderreport::print_status_block(std::cout, A);

    std::cout << "  Tags        : ";
    if (S.tags.empty()) {
        std::cout << "(none)\n";
    } else if (!verbose) {
        bool first = true;
        for (const auto& t : S.tags) {
            if (!first) std::cout << ", ";
            std::cout << (t.tagName.empty() ? t.fieldName : t.tagName);
            first = false;
        }
        std::cout << "\n";
    } else {
        std::cout << "\n\n";
        std::cout << "  Tags\n";
        std::cout
            << "  "
            << std::left << std::setw(14) << "Field Name"
            << std::setw(8)  << "Type"
            << std::setw(6)  << "Len"
            << std::setw(6)  << "Dec"
            << "Dir\n";
        std::cout << "  ------------ ------- ------ ------ ----\n";

        for (const auto& T : S.tags) {
            const std::string field_name = !T.fieldName.empty() ? T.fieldName : T.tagName;
            std::cout
                << "  "
                << std::left << std::setw(14) << field_name
                << std::setw(8)  << (T.type.empty() ? "" : T.type)
                << std::setw(6)  << T.len
                << std::setw(6)  << T.dec
                << (T.asc ? "ASC" : "DESC")
                << "\n";
        }
    }

    std::cout << "  Records     : " << A.recCount() << "\n";

    std::cout << "  LMDB        : ";
    const auto* im = A.indexManagerPtr();
    if (!im || !im->hasBackend() || !im->isCdx()) {
        std::cout << "(closed)\n";
    } else {
        const std::string envdir =
            im->containerPath().empty()
                ? std::string()
                : dottalk::paths::resolve_lmdb_env_for_cdx(im->containerPath()).string();
        const std::string tag = im->activeTag();

        std::cout << "envdir=" << (envdir.empty() ? std::string("(unknown)") : envdir);
        if (!tag.empty()) {
            std::cout << "  tag=" << tag;
        }
        std::cout << "\n";
    }
}

void print_struct(const xbase::DbArea& A) {
    const auto& F = A.fields();

    std::cout << "\nFields (" << F.size() << ")\n";
    std::cout
        << "  #  "
        << std::left << std::setw(12) << "Name"
        << std::setw(6)  << "Type"
        << std::setw(6)  << "Len"
        << std::setw(6)  << "Dec"
        << "\n";

    for (std::size_t i = 0; i < F.size(); ++i) {
        const auto& fd = F[i];
        std::cout
            << std::right << std::setw(4) << (i + 1) << "  "
            << std::left  << std::setw(12) << fd.name
            << std::setw(6)  << fd.type
            << std::setw(6)  << static_cast<int>(fd.length)
            << std::setw(6)  << static_cast<int>(fd.decimals)
            << "\n";
    }
}

void print_status_for_area(std::size_t slot, xbase::DbArea& A, bool isCurrent, bool verbose) {
    print_area_header(slot, A, isCurrent);
    print_workspace_line();
    print_index_body(A, verbose);
    if (verbose) {
        print_struct(A);
    }
}

} // namespace

void cmd_STATUS(xbase::DbArea& A, std::istringstream& args) {
    const std::string raw = upper(args.str());
    const bool wantAll = (raw.find("ALL") != std::string::npos);
    const bool wantVerbose = (raw.find("VERBOSE") != std::string::npos);

    if (!wantAll) {
        const std::size_t cur = workareas::current_slot();
        xbase::DbArea* curArea = (cur < workareas::count()) ? workareas::at(cur) : &A;

        if (!curArea || !curArea->isOpen()) {
            std::cout
                << "Area " << cur << ":   () [current]\n"
                << "Workspace : " << workareas::occupied_desc() << "\n"
                << "DBF Flavor   : unknown\n"
                << "Order       : PHYSICAL\n"
                << "  Tags        : (none)\n"
                << "  Records     : 0\n"
                << "  LMDB        : (closed)\n";
            return;
        }

        print_status_for_area(cur, *curArea, true, wantVerbose);
        return;
    }

    for (std::size_t i = 0; i < workareas::count(); ++i) {
        xbase::DbArea* ar = workareas::at(i);
        if (!is_open(ar)) continue;
        print_status_for_area(i, *ar, i == workareas::current_slot(), wantVerbose);
        std::cout << "\n";
    }
}

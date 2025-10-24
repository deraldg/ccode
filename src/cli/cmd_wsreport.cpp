#include <sstream>
#include <string>
#include <iostream>
#include <algorithm>

namespace xbase { class DbArea; }

extern void cmd_FIELDS (xbase::DbArea& area, std::istringstream& args);
extern void cmd_STATUS (xbase::DbArea& area, std::istringstream& args);

namespace dli {
    int  api_get_current_area(xbase::DbArea&);
    bool api_select_area(xbase::DbArea&, int target);
    bool api_area_is_open(xbase::DbArea&, int target);
    void api_area_ident(xbase::DbArea&, int target,
                        std::string& outAlias, std::string& outFile);
}

#ifndef DLI_SHELL_API_PROVIDED
namespace dli {
    inline int  api_get_current_area(xbase::DbArea& /*area*/) { return 1; }
    inline bool api_select_area(xbase::DbArea& /*area*/, int /*target*/) { return true; }
    inline bool api_area_is_open(xbase::DbArea& area, int target) { return target == api_get_current_area(area); }
    inline void api_area_ident(xbase::DbArea& /*area*/, int /*target*/,
                               std::string& outAlias, std::string& outFile) {
        outAlias.clear(); outFile.clear();
    }
}
#endif

static inline int clampi(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

static void run_wsreport(xbase::DbArea& areaRef, int startArea, int endArea) {
    const int start = clampi(startArea, 1, 26);
    const int stop  = clampi(endArea, 1, 26);

    const int savedArea = dli::api_get_current_area(areaRef);

    std::cout << "DotTalk Status Report\n";

    for (int areaIdx = start; areaIdx <= stop; ++areaIdx) {
        if (!dli::api_area_is_open(areaRef, areaIdx)) continue;
        if (!dli::api_select_area(areaRef, areaIdx))  continue;

        std::cout << "\nWorkspace (area " << areaIdx << ")\n";

        std::string alias, file;
        dli::api_area_ident(areaRef, areaIdx, alias, file);
        if (!alias.empty() || !file.empty()) {
            std::cout << "  " << (alias.empty() ? "(no alias)" : alias);
            if (!file.empty()) std::cout << "  " << file;
            std::cout << "\n";
        }

        std::cout << "Fields()\n";
        { std::istringstream sub(""); cmd_FIELDS(areaRef, sub); }

        std::cout << "Order/Index Info\n";
        { std::istringstream sub("INDEX"); cmd_STATUS(areaRef, sub); }

        std::cout << "Index Fields\n";
        { std::istringstream sub("INDEXFIELDS"); cmd_STATUS(areaRef, sub); }

        std::cout << "Indexed Formulas\n";
        { std::istringstream sub("INDEXFORMULAS"); cmd_STATUS(areaRef, sub); }
    }

    if (savedArea >= 1 && savedArea <= 26) {
        dli::api_select_area(areaRef, savedArea);
    }
}

void cmd_WSREPORT(xbase::DbArea& areaRef, std::istringstream& args) {
    int start = 1, stop = 26;
    if (args.good()) {
        if (args >> start) { if (!(args >> stop)) stop = 26; }
    }
    if (start > stop) std::swap(start, stop);
    run_wsreport(areaRef, start, stop);
}

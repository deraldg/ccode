// src/cli/cmd_refresh.cpp

#include "xbase.hpp"
#include "cli/order_state.hpp"   // src/cli/order_state.hpp (same folder as this .cpp)

#include <filesystem>
#include <iostream>
#include <sstream>
#include <system_error>

void cmd_REFRESH(xbase::DbArea& a, std::istringstream&) {
    if (!a.isOpen()) {
        std::cout << "No table open.\n";
        return;
    }

    // Canonical absolute DBF path (DbArea::open expects abs path)
    const std::string abs_dbf = a.filename();
    if (abs_dbf.empty()) {
        std::cout << "Refresh failed: current table has no filename().\n";
        return;
    }

    // Capture cursor + order state BEFORE reopening.
    const int32_t keep_recno = a.recno();

    const bool had_order = orderstate::hasOrder(a);
    const std::string order_container = had_order ? orderstate::orderName(a) : std::string{};
    const bool was_asc = orderstate::isAscending(a);

    const bool was_cnx = had_order ? orderstate::isCnx(a) : false;
    const std::string cnx_tag = (had_order && was_cnx) ? orderstate::activeTag(a) : std::string{};

    // Preflight existence so we don't accidentally close the area on a bad open attempt.
    {
        std::error_code ec;
        if (!std::filesystem::exists(abs_dbf, ec)) {
            std::cout << "Refresh failed: file not found: " << abs_dbf << "\n";
            return;
        }
    }

    try {
        // Reopen DBF (may clear existing order/index attachments internally).
        a.open(abs_dbf);

        // Restore record position (best-effort).
        if (keep_recno > 0 && keep_recno <= a.recCount()) {
            a.gotoRec(keep_recno);
        } else if (a.recCount() > 0) {
            a.top();
        }

        // Restore order state (best-effort; warn but keep DBF open if it fails).
        if (had_order && !order_container.empty()) {
            try {
                orderstate::setOrder(a, order_container);

                if (was_cnx && !cnx_tag.empty()) {
                    orderstate::setActiveTag(a, cnx_tag);
                }

                orderstate::setAscending(a, was_asc);
            } catch (const std::exception& e) {
                std::cout << "REFRESH warning: order restore failed: " << e.what() << "\n";
            }
        }

        std::cout << "Refreshed " << a.logicalName()
                  << " (" << a.recCount() << " records).\n";
    }
    catch (const std::exception& e) {
        std::cout << "Refresh failed: " << e.what() << "\n";
    }
}

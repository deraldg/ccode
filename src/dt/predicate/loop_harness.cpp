// src/dt/predicate/loop_harness.cpp
//
// Unified loop harness over DbArea records.
//

#include "dt/predicate/loop_harness.hpp"
#include "dt/predicate/predicate_engine.hpp"

#include "xbase.hpp"

namespace dt::predicate {

LoopResult loop_records(xbase::DbArea&    area,
                        const expr::Node* pred,
                        const LoopSpec&   spec,
                        const std::function<void(RecordContext&)>& action) {
    LoopResult res;

    if (!area.isOpen()) {
        res.aborted = true;
        res.last_error = "loop_records: area is not open.";
        return res;
    }

    int total = area.recCount();
    if (total <= 0) {
        return res; // nothing to do
    }

    int start = spec.start_recno > 0 ? spec.start_recno : 1;
    int end   = spec.end_recno > 0 ? spec.end_recno : total;

    if (start < 1) start = 1;
    if (end > total) end = total;
    if (start > end) {
        return res;
    }

    for (int r = start; r <= end; ++r) {
        if (!area.gotoRec(r) || !area.readCurrent()) {
            // Skip unreadable records.
            continue;
        }

        res.visited++;

        RecordContext rc;
        rc.area = &area;
        rc.recno = r;

        bool pass = true;
        std::string err;

        if (pred) {
            pass = record_matches(area, r, pred, &err);
            if (!pass && !err.empty() && spec.stop_on_error) {
                res.aborted = true;
                res.last_error = err;
                break;
            }
        }

        if (!pass) {
            continue;
        }

        res.matched++;

        // Invoke caller's action.
        action(rc);
        res.acted++;
    }

    return res;
}

} // namespace dt::predicate




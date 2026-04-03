#pragma once

#include <functional>
#include <string>

namespace xbase {
    class DbArea;
}

namespace expr {
    struct Node;
}

namespace dt::predicate {

struct RecordContext;

// Configuration for record loops (SCAN, COUNT, DELETE FOR, LIST FOR, etc.)
struct LoopSpec {
    int  start_recno{1};      // inclusive; 1 = first record
    int  end_recno{0};        // inclusive; 0 = "auto" (use recCount)
    bool stop_on_error{false};
};

// Result summary for a loop.
struct LoopResult {
    int  visited{0};          // records visited
    int  matched{0};          // predicate-true
    int  acted{0};            // action() invoked
    bool aborted{false};      // aborted due to error or early-exit
    std::string last_error;   // last error message, if any
};

// Unified loop harness: walks records in DbArea, evaluates the predicate,
// and invokes `action` for matches.
//
// - area:    work area to iterate
// - pred:    expression tree predicate (may be nullptr for "no filter")
// - spec:    loop configuration (start/end, stop_on_error, etc.)
// - action:  callback invoked as action(RecordContext& rc) on each match
//
// Returns a LoopResult with counts and error/abort status.
//
LoopResult loop_records(xbase::DbArea&    area,
                        const expr::Node* pred,
                        const LoopSpec&   spec,
                        const std::function<void(RecordContext&)>& action);

} // namespace dt::predicate




#include "browse_filters.hpp"

#include "xbase.hpp"
#include "cli/where_eval_shared.hpp"  // compile_where_expr_cached, run_program

namespace dottalk::browse::filters {

ForProgram compile_for(const std::string& expr) {
    if (expr.empty()) return ForProgram{nullptr};
    auto ce = where_eval::compile_where_expr_cached(expr);
    // If compile fails, return null and let caller treat it as "no FOR".
    return ForProgram{ce};
}

bool record_visible(::xbase::DbArea& db, const ForProgram& prog, bool deleted_on) {
    // Respect SET DELETED ON: hidden rows are not visible.
    if (deleted_on && db.isDeleted()) return false;

    // If we have a compiled FOR program, run it.
    if (prog.prog) {
        // run_program returns bool: true => keep row, false => filter out
        if (!where_eval::run_program(*prog.prog->plan, db)) return false;
    }
    return true;
}

} // namespace dottalk::browse::filters

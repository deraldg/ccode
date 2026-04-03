// include/where_hook.hpp
#pragma once
#include <string>

namespace xbase { class DbArea; }

namespace wherehook {

struct DebugInfo {
    std::string cli_value_searched;    // <cli_value_searched>
    std::string field_value_examined;  // <field_value_examined>
};

/** Evaluator of WHERE expressions. Returns comparison boolean. */
using EvalFn = bool(*)(xbase::DbArea& area, const std::string& expr, DebugInfo* dbg);

/** Field getter hook. Must fill `out` with the current record's field value and return true. */
using FieldGetter = bool(*)(xbase::DbArea& area, const std::string& field_name, std::string& out);

/** Set these from your DB/predicate module. The stub provides defaults; replace when wiring real engine. */
extern EvalFn     evaluator;
extern FieldGetter get_field;

} // namespace wherehook




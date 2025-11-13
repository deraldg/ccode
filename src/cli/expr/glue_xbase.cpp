#include "dottalk/expr/glue_xbase.hpp"
#include "dottalk/expr/eval.hpp"        // iequals, to_number
#include "xbase.hpp"                    // xbase::DbArea

#include <string>

namespace dottalk { namespace expr { namespace glue {

// Helper: find 1-based field index by case-insensitive name
static int find_field_index_ci(const xbase::DbArea& area, std::string_view name) {
  const auto& flds = area.fields();
  for (size_t i = 0; i < flds.size(); ++i) {
    if (iequals(flds[i].name, std::string(name))) return int(i) + 1; // 1-based
  }
  return 0;
}

RecordView make_record_view(xbase::DbArea& area) {
  RecordView rv;
  rv.get_field_str = [&](std::string_view name)->std::string {
    int idx = find_field_index_ci(area, name);
    if (idx <= 0) return std::string();
    return area.get(idx);
  };
  rv.get_field_num = [&](std::string_view name)->std::optional<double> {
    int idx = find_field_index_ci(area, name);
    if (idx <= 0) return std::nullopt;
    auto s = area.get(idx);
    return to_number(s); // numeric coercion similar to comparisons
  };
  return rv;
}

}}} // namespace dottalk::expr::glue

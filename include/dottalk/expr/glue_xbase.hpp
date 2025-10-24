#pragma once
// Minimal glue between DotTalk++ xbase::DbArea and the expr evaluator.
//
// Define these two macros BEFORE including this header to map to your API:
//   #define DOTTALK_GET_FIELD_STR(area, name)  (area.getFieldAsString(name))
//   #define DOTTALK_GET_FIELD_NUM(area, name)  (area.getFieldAsNumber(name))
//
// If not defined, defaults will return empty / nullopt (expressions will likely be false).

#include <optional>
#include <string>
#include <string_view>
#include "dottalk/expr/ast.hpp"

namespace dottalk { namespace expr { namespace glue {

template <typename DbArea>
inline RecordView make_record_view(DbArea& area) {
  RecordView rv;
  rv.get_field_str = [&](std::string_view name)->std::string {
  #ifdef DOTTALK_GET_FIELD_STR
    return DOTTALK_GET_FIELD_STR(area, std::string(name));
  #else
    (void)area; (void)name;
    return std::string{};
  #endif
  };
  rv.get_field_num = [&](std::string_view name)->std::optional<double> {
  #ifdef DOTTALK_GET_FIELD_NUM
    try {
      auto val = DOTTALK_GET_FIELD_NUM(area, std::string(name));
      return val;
    } catch(...) { return std::nullopt; }
  #else
    (void)area; (void)name;
    return std::nullopt;
  #endif
  };
  return rv;
}

}}} // namespace

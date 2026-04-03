#pragma once
#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

namespace dottalk { namespace expr {

inline std::string up(std::string s) {
  for (auto& c: s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}

inline bool iequals(std::string_view a, std::string_view b) {
  if (a.size()!=b.size()) return false;
  for (size_t i=0;i<a.size();++i) {
    unsigned char ca = static_cast<unsigned char>(a[i]);
    unsigned char cb = static_cast<unsigned char>(b[i]);
    if (std::toupper(ca)!=std::toupper(cb)) return false;
  }
  return true;
}

inline std::optional<double> to_number(std::string_view s) {
  try {
    size_t idx=0;
    std::string tmp(s);
    double v = std::stod(tmp, &idx);
    if (idx != tmp.size()) return std::nullopt;
    return v;
  } catch(...) { return std::nullopt; }
}

}} // namespace




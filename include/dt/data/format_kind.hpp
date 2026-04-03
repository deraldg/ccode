#pragma once
#include <string>
#include <vector>

namespace dt::data {

enum class FormatKind {
    Unknown = 0,
    DBF,
    CSV,
    FIXED,
    TSV,
    JSON,
    XML
};

inline const char* format_kind_name(FormatKind k)
{
    switch (k) {
    case FormatKind::DBF:   return "DBF";
    case FormatKind::CSV:   return "CSV";
    case FormatKind::FIXED: return "FIXED";
    case FormatKind::TSV:   return "TSV";
    case FormatKind::JSON:  return "JSON";
    case FormatKind::XML:   return "XML";
    default:                return "UNKNOWN";
    }
}

struct FormatInfo {
    FormatKind kind = FormatKind::Unknown;
    std::string display_name;
    std::vector<std::string> extensions;
    bool can_read = false;
    bool can_write = false;
};

} // namespace dt::data
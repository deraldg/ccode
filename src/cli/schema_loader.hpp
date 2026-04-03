#pragma once
#include <string>
#include <vector>
#include <optional>

namespace dottalk {

struct SchemaField {
    std::string name;
    std::string kind;   // BASE|EXT|MEMO|DERIVED
    std::string label;
    std::string owner;
};

struct LogicalSchema {
    std::vector<SchemaField> fields;
    std::string source;           // "ddl.json" | "header" | "fallback"
    std::vector<std::string> warnings;
};

class SchemaResolver {
public:
    static LogicalSchema resolve(const std::string& area_name);
};

class SidecarLoader {
public:
    static std::optional<std::string> load_json_sidecar(const std::string& area_name);
};

} // namespace dottalk

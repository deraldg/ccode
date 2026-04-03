#include "schema_loader.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "xbase.hpp"
#include "workareas.hpp"

#if __has_include("cli/path_resolver.hpp") && __has_include("cli/cmd_setpath.hpp")
  #include "cli/path_resolver.hpp"
  #include "cli/cmd_setpath.hpp"
  #define HAVE_PATHS 1
#else
  #define HAVE_PATHS 0
#endif

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

inline xbase::DbArea* current_area() {
    try {
        const std::size_t slot = workareas::current_slot();
        return const_cast<xbase::DbArea*>(workareas::at(slot));
    } catch (...) {
        return nullptr;
    }
}

static bool file_exists(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p, ec) && fs::is_regular_file(p, ec);
}

#if HAVE_PATHS
namespace paths = dottalk::paths;
static inline fs::path schemas_root() { return paths::get_slot(paths::Slot::SCHEMAS); }
#else
static inline fs::path schemas_root() { return fs::current_path(); }
#endif

static fs::path resolve_schema_sidecar(const std::string& area_name,
                                       const std::string& suffix)
{
    const fs::path p1 = schemas_root() / (area_name + suffix);
    if (file_exists(p1)) return p1;

    const fs::path p2 = fs::current_path() / (area_name + suffix);
    if (file_exists(p2)) return p2;

    return p1;
}

} // namespace

namespace dottalk {

LogicalSchema SchemaResolver::resolve(const std::string& area_name) {
    LogicalSchema out;

    const fs::path ddl = resolve_schema_sidecar(area_name, ".ddl.json");
    if (file_exists(ddl)) {
        try {
            std::ifstream f(ddl);
            json j = json::parse(f, nullptr, true, true);
            if (j.contains("fields") && j["fields"].is_array()) {
                for (const auto& fld : j["fields"]) {
                    SchemaField s;
                    if (fld.contains("name"))       s.name  = fld["name"].get<std::string>();
                    if (fld.contains("label"))      s.label = fld["label"].get<std::string>();
                    if (fld.contains("kind"))       s.kind  = fld["kind"].get<std::string>();
                    if (fld.contains("owner_area")) s.owner = fld["owner_area"].get<std::string>();
                    if (s.name.empty()) continue;
                    if (s.kind.empty()) s.kind = "BASE";
                    out.fields.push_back(std::move(s));
                }
            }
            out.source = "ddl.json";
            return out;
        } catch (const std::exception& ex) {
            out.source = "fallback";
            out.warnings.push_back(std::string("DDL sidecar invalid: ") + ex.what());
            return out;
        }
    }

    if (auto* A = current_area()) {
        try {
            const auto& vec = A->fields();
            out.fields.reserve(vec.size());
            for (const auto& meta : vec) {
                SchemaField s;
                s.name  = meta.name;
                s.kind  = "BASE";
                s.label = meta.name;
                s.owner = area_name;
                out.fields.push_back(std::move(s));
            }
            out.source = "header";
            return out;
        } catch (...) {
        }
    }

    out.source = "fallback";
    out.warnings.push_back("No schema. Raw record view only.");
    return out;
}

std::optional<std::string> SidecarLoader::load_json_sidecar(const std::string& area_name) {
    const fs::path loadp = resolve_schema_sidecar(area_name, ".load.json");
    if (!file_exists(loadp)) return std::nullopt;
    try {
        std::ifstream f(loadp);
        json j = json::parse(f, nullptr, true, true);
        return j.dump();
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace dottalk
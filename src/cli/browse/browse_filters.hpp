#pragma once
#include <memory>
#include <string>

namespace xbase { class DbArea; }

// Forward decl to avoid heavy includes here.
namespace where_eval { struct CacheEntry; }

namespace dottalk::browse::filters {

struct ForProgram {
    // If compiled via where_eval; null if compile failed or empty expr.
    std::shared_ptr<const where_eval::CacheEntry> prog;
    // Always keep the original (possibly implicit) FOR text for simple fallback.
    std::string original_expr;
};

// Compile the FOR expression; empty/invalid → prog == nullptr (fallback may still run).
ForProgram compile_for(const std::string& expr);

// Visibility check used by BROWSE:
//  - respects SET DELETED (caller passes deleted_on)
//  - tries compiled program if present
//  - otherwise tries a simple fast-path for `field = value`
bool record_visible(::xbase::DbArea& db, const ForProgram& prog, bool deleted_on);

} // namespace dottalk::browse::filters

#pragma once

#include <memory>
#include <string>

namespace xbase { class DbArea; }
namespace dottalk::expr { struct Expr; }

namespace filter {

// Install or replace the persistent SET FILTER expression for this area.
bool set(xbase::DbArea* area, const std::string& text, std::string& err);

// Clear the persistent SET FILTER expression for this area.
void clear(xbase::DbArea* area);

// True if this area currently has a compiled persistent filter.
bool has(xbase::DbArea* area);

// True if this area has any active persistent filter state.
// This is the query used by navigation redirection.
bool has_active_filter(xbase::DbArea* area);

// Visibility gate used by LIST / COUNT / SMARTLIST / LOCATE and now nav redirect.
// Applies persistent SET FILTER and optional FOR expression.
bool visible(xbase::DbArea* area,
             const std::shared_ptr<dottalk::expr::Expr>& for_ast);

} // namespace filter
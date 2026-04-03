#pragma once
#include <string>

namespace xbase { class DbArea; }
namespace dottalk::browse { struct BrowseArgs; }

namespace dottalk::browse::io {

bool prompt_more(bool quiet);
void print_banner(::xbase::DbArea& area, const BrowseArgs& args, const std::string& order_summary);
void print_status(::xbase::DbArea& area, const std::string& order_summary, bool dirty);
void print_help_inline();

} // namespace dottalk::browse::io

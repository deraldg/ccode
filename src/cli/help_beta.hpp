#pragma once
#include <string>

namespace dottalk::help {

// Render BETA checklist or detail.
//  - term empty => full checklist
//  - term like "BETA-3.1" => details for that item
void show_beta(const std::string& term_upper_or_raw);

} // namespace dottalk::help

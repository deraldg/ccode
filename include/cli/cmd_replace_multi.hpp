#pragma once
#include <string>
#include <vector>
#include <sstream>

namespace xbase { class DbArea; }

// Shared lightweight tuple used across TUs (e.g., TUI staged editors)
struct FieldUpdate {
    std::string name;
    std::string value;
};

// Helper overload used by non-CLI code (e.g., transactional saves)
bool cmd_REPLACE_MULTI(xbase::DbArea& area,
                       const std::vector<FieldUpdate>& updates,
                       std::string* error);

// CLI entrypoint (bind this to "MULTIREP" in the registry; REPLACE can stay single-field)
void cmd_REPLACE_MULTI(xbase::DbArea& area, std::istringstream& args);




#pragma once
#include <string>
#include <vector>

namespace xbase { class DbArea; }

struct FieldUpdate {
    std::string name;
    std::string value; // raw or normalized text; validation happens in the command
};

// Returns true on success; on failure fills *error (if not null)
bool cmd_REPLACE_MULTI(xbase::DbArea&, const std::vector<FieldUpdate>& updates, std::string* error = nullptr);




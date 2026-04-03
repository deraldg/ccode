#pragma once
#include <map>
#include <string>

namespace xbase { class DbArea; }

namespace dottalk::browse::edit {

using StageMap = std::map<int, std::string>;

int field_index_by_name(::xbase::DbArea& db, const std::string& name);
void list_fields(::xbase::DbArea& db);

bool commit_staged(::xbase::DbArea& db, StageMap& staged);
void discard_staged(StageMap& staged);

} // namespace dottalk::browse::edit

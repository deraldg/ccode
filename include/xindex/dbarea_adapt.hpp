#pragma once
#include <string>
namespace xbase { class DbArea; }

namespace xindex {
std::string db_get_string (const xbase::DbArea& a, int recno, const std::string& field);
double      db_get_double (const xbase::DbArea& a, int recno, const std::string& field);
int         db_record_count(const xbase::DbArea& a);
bool        db_is_deleted (const xbase::DbArea& a, int recno);
void        db_goto_rec   (      xbase::DbArea& a, int recno);
std::string db_current_dbf_path(const xbase::DbArea& a);  // may return ""
} // namespace xindex

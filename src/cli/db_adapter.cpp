#include <string>
namespace xbase { class DbArea; }
extern "C" { xbase::DbArea* cli_current_area_get(); }
static inline xbase::DbArea& A(){ return *cli_current_area_get(); }

namespace xindex {
  bool db_move_top(xbase::DbArea&); bool db_move_next(xbase::DbArea&);
  bool db_at_eof(const xbase::DbArea&); bool db_record_is_deleted(const xbase::DbArea&);
  bool db_delete_current(xbase::DbArea&); bool db_position_valid(const xbase::DbArea&);
  void db_render_current(const xbase::DbArea&);
  bool db_index_has_active(const xbase::DbArea&);
  bool db_seek_key(const std::string&, bool, xbase::DbArea&);
  bool db_index_clear(xbase::DbArea&); bool db_index_load_file(const std::string&, xbase::DbArea&);
  bool db_index_set_order(const std::string&, xbase::DbArea&);
  int db_index_tag_count(const xbase::DbArea&); const char* db_index_active_tag(const xbase::DbArea&);
  std::string db_current_table_path(const xbase::DbArea&);
  int db_record_count(const xbase::DbArea&); int db_record_length(const xbase::DbArea&);
  int db_current_recno(const xbase::DbArea&); bool db_order_descending(const xbase::DbArea&);
  bool db_field_get_char(const std::string&, std::string&, const xbase::DbArea&);
  bool db_field_get_numeric(const std::string&, double&, const xbase::DbArea&);
  bool db_field_get_logical(const std::string&, bool&, const xbase::DbArea&);
  bool db_field_get_date_yyyymmdd(const std::string&, std::string&, const xbase::DbArea&);
}

bool db_move_top(){ return xindex::db_move_top(A()); }
bool db_move_next(){ return xindex::db_move_next(A()); }
bool db_at_eof(){ return xindex::db_at_eof(A()); }
bool db_record_is_deleted(){ return xindex::db_record_is_deleted(A()); }
bool db_delete_current(){ return xindex::db_delete_current(A()); }
bool db_position_valid(){ return xindex::db_position_valid(A()); }
void db_render_current(){ xindex::db_render_current(A()); }
bool db_index_has_active(){ return xindex::db_index_has_active(A()); }
bool db_seek_key(const std::string& k,bool d){ return xindex::db_seek_key(k,d,A()); }
bool db_index_clear(){ return xindex::db_index_clear(A()); }
bool db_index_load_file(const std::string& p){ return xindex::db_index_load_file(p,A()); }
bool db_index_set_order(const std::string& t){ return xindex::db_index_set_order(t,A()); }
int  db_index_tag_count(){ return xindex::db_index_tag_count(A()); }
const char* db_index_active_tag(){ return xindex::db_index_active_tag(A()); }
std::string db_current_table_path(){ return xindex::db_current_table_path(A()); }
int  db_record_count(){ return xindex::db_record_count(A()); }
int  db_record_length(){ return xindex::db_record_length(A()); }
int  db_current_recno(){ return xindex::db_current_recno(A()); }
bool db_order_descending(){ return xindex::db_order_descending(A()); }
bool db_field_get_char(const std::string& n, std::string& out){ return xindex::db_field_get_char(n,out,A()); }
bool db_field_get_numeric(const std::string& n, double& out){ return xindex::db_field_get_numeric(n,out,A()); }
bool db_field_get_logical(const std::string& n, bool& out){ return xindex::db_field_get_logical(n,out,A()); }
bool db_field_get_date_yyyymmdd(const std::string& n, std::string& out){ return xindex::db_field_get_date_yyyymmdd(n,out,A()); }




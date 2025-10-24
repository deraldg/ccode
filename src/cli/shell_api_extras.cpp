#include <sstream>
namespace xbase { class DbArea; }
void cmd_STATUS(xbase::DbArea&, std::istringstream&);
void run_wsreport(xbase::DbArea& a, int, int){ std::istringstream none; cmd_STATUS(a, none); }

#include "relations_status.hpp"

#if __has_include("set_relations.hpp")
  #include "set_relations.hpp"
#endif

namespace relations_status {

#if __has_include("set_relations.hpp")
static inline std::string default_token() { return "REL: configured"; }
#else
static inline std::string default_token() { return "REL: none"; }
#endif

static inline std::vector<ChildStat> default_details() { return {}; }

#ifdef SUPERBROWSER_REL_STATUS_HOOK
  std::string relation_status_token() { return SUPERBROWSER_REL_STATUS_HOOK(); }
#else
  std::string relation_status_token() { return default_token(); }
#endif

#ifdef SUPERBROWSER_REL_DETAILS_HOOK
  std::vector<ChildStat> relation_stats_for_current_parent() { return SUPERBROWSER_REL_DETAILS_HOOK(); }
#else
  std::vector<ChildStat> relation_stats_for_current_parent() { return default_details(); }
#endif

} // namespace relations_status

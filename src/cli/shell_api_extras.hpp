#ifndef DOTTALK_CLI_SHELL_API_EXTRAS_HPP
#define DOTTALK_CLI_SHELL_API_EXTRAS_HPP

#include <string>

namespace cli {

// Rewrite multi-word lead-ins (case-insensitive) to first-token forms
// e.g., "SET RELATIONS ADD ..." -> "REL ADD ..."
//       "RELATIONS LIST"        -> "REL LIST"
std::string preprocess_for_dispatch(const std::string& line);

} // namespace cli

#endif // DOTTALK_CLI_SHELL_API_EXTRAS_HPP

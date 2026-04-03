#pragma once
#include <string>
#include <optional>

namespace xbase { class DbArea; }

namespace dli {

// Use the real REPLACE command handler to update a single field with raw text.
// Returns true on success; on failure returns false and optionally sets errorOut.
bool do_replace_text(xbase::DbArea& db,
                     const std::string& fieldName,
                     const std::string& rawUserText,
                     std::string* errorOut = nullptr);

// Memo-friendly path: writes content to a transient file and invokes
//   REPLACE <field> WITH FILE "<temp_path>"
// This avoids escaping/length pitfalls for large memo text.
bool do_replace_memo_text(xbase::DbArea& db,
                          const std::string& fieldName,
                          const std::string& memoText,
                          std::string* errorOut = nullptr);

} // namespace dli




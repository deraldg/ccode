#pragma once
#include <sstream>

namespace xbase { class DbArea; }

// DotTalk++ command: SCHEMA
//
// Supported syntax:
//
//   SCHEMA FETCH <url> TO <file> [OVERWRITE]
//
//   SCHEMA VALIDATE <schema.json> USING <validator.json>
//
//   SCHEMA CREATE DBF <out.dbf> FROM <schema.json> [OVERWRITE]
//       [SEED CSV <path.csv> | SEED BLANK <N>]
//       [REJECTS <rejects.csv>] [EMIT SIDECARS]
//
// Path rules:
// - Relative schema/validator inputs resolve under the SCHEMAS slot first.
// - Relative FETCH outputs resolve under the SCHEMAS slot.
// - Relative CREATE DBF outputs resolve under the TMP slot by default.
// - Absolute paths are honored as-is.
void cmd_SCHEMA(xbase::DbArea& area, std::istringstream& iss);
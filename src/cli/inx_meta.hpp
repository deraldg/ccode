#pragma once
// inx_meta.hpp ? DotTalk++ INX metadata reader (header-only interface)
// Reads the lightweight header of a ".inx" index file without touching writers.
// Format (v1):
//   Bytes 0..3   : "1INX"
//   uint16_le    : version (currently 1)
//   uint16_le    : exprNameLen
//   bytes[..]    : exprName (key expression token, raw bytes, no NUL required)
//   uint32_le    : entryCount
//
// No dependency on index writers or managers.
// Safe to use from cmd_browse/cmd_status/cmd_order/etc.
//
// Copyright: DotTalk++
// License  : Project-internal (same as core)

#include <cstdint>
#include <string>

namespace dottalk {
namespace inx {

struct Meta {
    bool        ok{false};        // header parsed and magic matched
    std::string path;             // file path passed in
    std::string key_expr;         // raw expr token as written by the indexer
    std::uint16_t version{0};     // header version
    std::uint32_t entry_count{0}; // number of entries claimed by header
    std::string error;            // reason if ok==false (for diagnostics)
};

// Reads only the header; does not scan entries.
// Robust to short/truncated files and returns Meta{ok=false, error=...} on failure.
Meta read_inx_header(const std::string& path);

} // namespace inx
} // namespace dottalk




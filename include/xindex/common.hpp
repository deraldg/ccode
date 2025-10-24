#pragma once
#include "xindex/key_common.hpp"
#include <fstream>
#include <ios>

// ... rest of your code



namespace xindex {

inline constexpr char kIndexKind[]     = "IDX";   // family identifier
inline constexpr char kDefaultExt[]    = ".idx";  // canonical extension
inline constexpr uint32_t kFileVersion = 1;

// Name for the *persistent* backend in this bundle
inline constexpr char kBackendKind_BPTREE[] = "BPTREE";
// Name for the *in-memory* backend in this bundle
inline constexpr char kBackendKind_BPTMEM[] = "BPTMEM";

struct Fingerprint {
    uint32_t codec_version{1};  // bump when key encoding changes
    uint32_t cpr{0};            // bytes per record (from DBF header), optional
    uint32_t field_count{0};    // number of fields used in key, optional
    uint32_t key_hash{0};       // hash of key expression/fields, optional
};

} // namespace xindex

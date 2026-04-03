#pragma once
// Plan A: Minimal field metadata POD used by codecs/packer.
// Keep this header-only and free of external dependencies.
#include <string>
#include <cstddef>

namespace cli_planA {

struct FieldMeta {
    std::string name;   // uppercase, ASCII (DBF header uses ASCII names up to 11 bytes)
    char        type{}; // 'C','N','F','D','L','M','G','B' etc.
    int         length{0};
    int         decimal{0};
    std::size_t offset{0}; // byte offset from START OF DATA (i.e., AFTER the deleted-flag)
};

} // namespace cli_planA




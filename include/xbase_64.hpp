// xbase_64.hpp
// DotTalk++ large-file / 64-bit dialect — version 0x64

#pragma once

#include "xbase.hpp"
#include "xbase_vfp.hpp"

#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace xbase {

constexpr uint8_t DBF_VERSION_64 = 0x64;

// -----------------------------------------------------------------------------
// 64-BYTE EXTENSION BLOCK (0x64 DIALECT)
// -----------------------------------------------------------------------------
#pragma pack(push, 1)
struct LargeHeaderExtension {
    uint64_t record_count;
    uint64_t data_start_64;
    uint64_t record_size_64;
    uint64_t autoq_next;
    uint32_t table_flags;
    uint32_t reserved32;
    uint64_t reserved[3];
};
#pragma pack(pop)

static_assert(sizeof(LargeHeaderExtension) == 64,
              "LargeHeaderExtension must be exactly 64 bytes");

constexpr uint16_t LARGE_HEADER_SIZE =
    32 + static_cast<uint16_t>(sizeof(LargeHeaderExtension));

// -----------------------------------------------------------------------------
// TABLE FLAGS
// -----------------------------------------------------------------------------
constexpr uint32_t DBF64_FLAG_HAS_MEMO     = 0x00000001;
constexpr uint32_t DBF64_FLAG_HAS_RECID_PK = 0x00000002;

// -----------------------------------------------------------------------------
// 0x64 LOADER
// -----------------------------------------------------------------------------
namespace x64_loader {

inline void readHeader(DbArea& area, std::fstream& fp) {
    const uint8_t ver = vfp_loader::peekVersion(fp);
    if (ver != DBF_VERSION_64) {
        throw std::runtime_error("Not an xbase_64 file (expected 0x64)");
    }

    area.setVersionByte(ver);
    area.setKind(detect_area_kind_from_version(ver));

    // --- read classic 32-byte VFP header ---
    VfpHeader vh{};
    fp.read(reinterpret_cast<char*>(&vh), sizeof(vh));
    if (!fp) {
        throw std::runtime_error("Failed to read base 32-byte x64 header");
    }

    area.setVersionByte(vh.version);
    area.setKind(detect_area_kind_from_version(vh.version));
    area.setLastUpdated(vh.yy, vh.mm, vh.dd);

    // --- read 64-byte extension ---
    LargeHeaderExtension ext{};
    fp.read(reinterpret_cast<char*>(&ext), sizeof(ext));
    if (!fp) {
        throw std::runtime_error("Truncated x64 64-byte extension");
    }

    // --- record count: clamp into 32-bit view for now ---
    const uint64_t max32 = static_cast<uint64_t>(std::numeric_limits<int32_t>::max());
    const int32_t compat_count =
        (ext.record_count > max32)
            ? std::numeric_limits<int32_t>::max()
            : static_cast<int32_t>(ext.record_count);

    area.setRecordCount(compat_count);

    // --- data start ---
    const int16_t ds =
        (ext.data_start_64 >= 32u && ext.data_start_64 <= 65535u)
            ? static_cast<int16_t>(ext.data_start_64)
            : static_cast<int16_t>(vh.header_size);

    // --- record size ---
    const int16_t rs =
        (ext.record_size_64 >= 1u && ext.record_size_64 <= 65535u)
            ? static_cast<int16_t>(ext.record_size_64)
            : static_cast<int16_t>(vh.record_size);

    area.setDataStart(ds);
    area.setRecordLength(rs);

    // --- new 64-bit fields ---
    area.setAutoQNext64(ext.autoq_next);
    area.setTableFlags(ext.table_flags);
}

inline void readFields(DbArea& area,
                       std::fstream& fp,
                       std::vector<VfpFieldExtras>& extras)
{
    fp.clear();
    fp.seekg(LARGE_HEADER_SIZE, std::ios::beg);
    if (!fp) {
        throw std::runtime_error("Failed to seek to x64 field descriptor area");
    }

    vfp_loader::readFields(area, fp, extras);
}

} // namespace x64_loader

} // namespace xbase
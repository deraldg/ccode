// xbase_vfp.hpp
// FoxPro / Visual FoxPro extensions — include only where needed

#pragma once

#include "xbase.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace xbase {

// ------------------------------------------------------------------------
// VFP-specific field extras (not baked into FieldDef)
// ------------------------------------------------------------------------
struct VfpFieldExtras {
    bool        nullable      {false};
    bool        binary        {false};
    bool        autoincrement {false};
    uint32_t    next_autoinc  {0};
    uint8_t     step_autoinc  {0};
    std::string long_name     {};
};

// ------------------------------------------------------------------------
// DBF level / flavor detection
// ------------------------------------------------------------------------
enum class DbfLevel : uint8_t {
    ClassicNoMemo   = 0x03,
    ClassicWithMemo = 0x83,
    Fox26Memo       = 0xF5,
    VfpBase         = 0x30,
    VfpAutoInc      = 0x31,
    VfpVar          = 0x32
};

inline std::string versionHex(uint8_t version)
{
    std::ostringstream oss;
    oss << "0x"
        << std::uppercase
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<unsigned int>(version);
    return oss.str();
}

inline DbfLevel detectDbfLevel(uint8_t version) {
    switch (version) {
        case 0x03: return DbfLevel::ClassicNoMemo;
        case 0x83: return DbfLevel::ClassicWithMemo;
        case 0xF5: return DbfLevel::Fox26Memo;
        case 0x30: return DbfLevel::VfpBase;
        case 0x31: return DbfLevel::VfpAutoInc;
        case 0x32: return DbfLevel::VfpVar;
        default:
            throw std::runtime_error("Unsupported DBF version: " + versionHex(version));
    }
}

inline bool isVfp(DbfLevel lvl) noexcept {
    return lvl == DbfLevel::VfpBase ||
           lvl == DbfLevel::VfpAutoInc ||
           lvl == DbfLevel::VfpVar;
}

inline bool isClassicLike(DbfLevel lvl) noexcept {
    return lvl == DbfLevel::ClassicNoMemo ||
           lvl == DbfLevel::ClassicWithMemo ||
           lvl == DbfLevel::Fox26Memo;
}

// ------------------------------------------------------------------------
// VFP header (32 bytes)
// ------------------------------------------------------------------------
#pragma pack(push, 1)
struct VfpHeader {
    uint8_t     version;
    uint8_t     yy;
    uint8_t     mm;
    uint8_t     dd;
    uint32_t    num_recs;
    uint16_t    header_size;
    uint16_t    record_size;
    uint8_t     reserved1[2];
    uint8_t     in_transaction;
    uint8_t     encrypted;
    uint8_t     reserved2[12];
    uint8_t     table_flags;
    uint8_t     codepage;
    uint16_t    reserved3;
};
#pragma pack(pop)

static_assert(sizeof(VfpHeader) == 32, "VfpHeader must be 32 bytes");

// ------------------------------------------------------------------------
// VFP field descriptor (32 bytes)
// ------------------------------------------------------------------------
#pragma pack(push, 1)
struct VfpField {
    char        name[11];
    char        type;
    uint32_t    displacement;
    uint8_t     length;
    uint8_t     decimals;
    uint16_t    reserved1;
    uint8_t     workarea;
    uint16_t    reserved2;
    uint8_t     flags;
    uint8_t     reserved3[8];
};
#pragma pack(pop)

static_assert(sizeof(VfpField) == 32, "VfpField must be 32 bytes");

// ------------------------------------------------------------------------
// Version-aware loader helpers
// ------------------------------------------------------------------------
namespace vfp_loader {

// Peek version without consuming stream position.
inline uint8_t peekVersion(std::fstream& fp) {
    const std::streampos pos = fp.tellg();
    uint8_t ver = 0;
    fp.read(reinterpret_cast<char*>(&ver), 1);
    if (!fp) throw std::runtime_error("Cannot peek DBF version byte");
    fp.clear();
    fp.seekg(pos);
    return ver;
}

// Read header and map to DbArea's encapsulated state.
inline void readHeader(DbArea& area, std::fstream& fp) {
    const uint8_t ver = peekVersion(fp);
    area.setVersionByte(ver);
    area.setKind(detect_area_kind_from_version(ver));

    if (ver == 0x03 || ver == 0x83 || ver == 0xF5) {
        HeaderRec h{};
        fp.read(reinterpret_cast<char*>(&h), sizeof(h));
        if (!fp) throw std::runtime_error("Classic/Fox26 header read failed");
        area.setHeader(h);
        area.setVersionByte(h.version);
        area.setKind(detect_area_kind_from_version(h.version));
        return;
    }

    if (ver == 0x30 || ver == 0x31 || ver == 0x32) {
        VfpHeader vh{};
        fp.read(reinterpret_cast<char*>(&vh), sizeof(vh));
        if (!fp) throw std::runtime_error("VFP header read failed");

        area.setVersionByte(vh.version);
        area.setKind(detect_area_kind_from_version(vh.version));
        area.setLastUpdated(vh.yy, vh.mm, vh.dd);
        area.setRecordCount(static_cast<int32_t>(vh.num_recs));
        area.setDataStart(static_cast<int16_t>(vh.header_size));
        area.setRecordLength(static_cast<int16_t>(vh.record_size));
        return;
    }

    throw std::runtime_error("Unsupported DBF version: " + versionHex(ver));
}

// Read field descriptors until the 0x0D terminator.
inline void readFields(DbArea& area,
                       std::fstream& fp,
                       std::vector<VfpFieldExtras>& extras)
{
    extras.clear();
    area.clearFields();

    const bool is_vfp = (area.versionByte() == 0x30 ||
                         area.versionByte() == 0x31 ||
                         area.versionByte() == 0x32);

    while (true) {
        const std::streampos pos = fp.tellg();

        uint8_t marker = 0;
        fp.read(reinterpret_cast<char*>(&marker), 1);
        if (!fp) {
            throw std::runtime_error("Truncated DBF header while reading fields");
        }

        if (marker == HEADER_TERM_BYTE) {
            break;
        }

        fp.clear();
        fp.seekg(pos);

        if (is_vfp) {
            VfpField vf{};
            fp.read(reinterpret_cast<char*>(&vf), sizeof(vf));
            if (!fp) {
                throw std::runtime_error("Truncated VFP field descriptor");
            }

            FieldDef fd;
            fd.name     = std::string(vf.name, strnlen(vf.name, 11));
            fd.type     = vf.type;
            fd.length   = vf.length;
            fd.decimals = vf.decimals;
            area.addField(std::move(fd));

            FieldRec fr{};
            std::memset(&fr, 0, sizeof(fr));
            const std::string& nm = area.fields().back().name;
            if (!nm.empty()) {
                const size_t copy_n = (nm.size() > 10) ? 10 : nm.size();
                std::memcpy(fr.field_name, nm.data(), copy_n);
            }
            fr.field_type         = area.fields().back().type;
            fr.field_data_address = 0;
            fr.field_length       = area.fields().back().length;
            fr.decimal_places     = area.fields().back().decimals;
            area.addRawField(std::move(fr));

            VfpFieldExtras ex;
            ex.nullable      = (vf.flags & 0x02) != 0;
            ex.binary        = (vf.flags & 0x04) != 0;
            ex.autoincrement = false;
            ex.next_autoinc  = 0;
            ex.step_autoinc  = 0;
            extras.push_back(std::move(ex));
        } else {
            FieldRec fr{};
            fp.read(reinterpret_cast<char*>(&fr), sizeof(fr));
            if (!fp) {
                throw std::runtime_error("Truncated classic field descriptor");
            }

            area.addRawField(fr);

            FieldDef fd;
            fd.name     = std::string(fr.field_name, strnlen(fr.field_name, 11));
            fd.type     = fr.field_type;
            fd.length   = fr.field_length;
            fd.decimals = fr.decimal_places;
            area.addField(std::move(fd));

            extras.push_back(VfpFieldExtras{});
        }
    }

    if (is_vfp) {
        fp.seekg(263, std::ios::cur);
        if (!fp) {
            throw std::runtime_error("Failed to skip VFP backlink block");
        }
    }
}

} // namespace vfp_loader

} // namespace xbase
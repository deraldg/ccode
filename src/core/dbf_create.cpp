#include "xbase/dbf_create.hpp"

#include "xbase.hpp"
#include "xbase_vfp.hpp"
#include "xbase_64.hpp"
#include "foxpro_header.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <system_error>
#include <vector>

namespace xbase::dbf_create {
namespace {

using foxpro_header::TableFlavor;

static inline void put_u32_le(std::ostream& o, std::uint32_t v)
{
    o.put((char)(v & 0xFF));
    o.put((char)((v >> 8) & 0xFF));
    o.put((char)((v >> 16) & 0xFF));
    o.put((char)((v >> 24) & 0xFF));
}

static inline void put_u32_be(std::ostream& o, std::uint32_t v)
{
    o.put((char)((v >> 24) & 0xFF));
    o.put((char)((v >> 16) & 0xFF));
    o.put((char)((v >> 8) & 0xFF));
    o.put((char)(v & 0xFF));
}

static inline void put_u16_be(std::ostream& o, std::uint16_t v)
{
    o.put((char)((v >> 8) & 0xFF));
    o.put((char)(v & 0xFF));
}

static std::string replace_ext_local(const std::string& p, const char* newext)
{
    const auto dot = p.find_last_of('.');
    if (dot == std::string::npos) return p + newext;
    return p.substr(0, dot) + newext;
}

static bool create_dbt_seed(const std::string& dbfPath, std::string& err)
{
    const std::string memo = replace_ext_local(dbfPath, ".dbt");
    std::ofstream m(memo, std::ios::binary | std::ios::trunc);
    if (!m) {
        err = "could not create DBT sidecar";
        return false;
    }

    std::uint32_t next = 1;
    m.write(reinterpret_cast<const char*>(&next), sizeof(next));
    if (!m) {
        err = "could not seed DBT header";
        return false;
    }

    m.seekp(512 - 1);
    char z = 0;
    m.write(&z, 1);
    if (!m) {
        err = "could not pad DBT header";
        return false;
    }
    return true;
}

static bool create_fpt_seed(const std::string& dbfPath, std::string& err)
{
    const std::string memo = replace_ext_local(dbfPath, ".fpt");
    std::ofstream m(memo, std::ios::binary | std::ios::trunc);
    if (!m) {
        err = "could not create FPT sidecar";
        return false;
    }

    put_u32_be(m, 1u);   // next free block
    put_u16_be(m, 64u);  // block size

    std::array<char, 512 - 6> rest{};
    m.write(rest.data(), static_cast<std::streamsize>(rest.size()));
    if (!m) {
        err = "could not seed FPT header";
        return false;
    }
    return true;
}

static bool write_classic_dbf(const std::string& path,
                              const std::vector<FieldSpec>& fields,
                              TableFlavor flavor,
                              std::string& err)
{
    bool hasMemo = false;
    std::uint16_t recLen = 1; // delete flag

    for (const auto& f : fields)
    {
        recLen = static_cast<std::uint16_t>(recLen + f.len);
        if (f.type == 'M') hasMemo = true;
    }

    const std::uint16_t hdrLen =
        static_cast<std::uint16_t>(32 + fields.size() * 32 + 1);

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        err = "could not create DBF file";
        return false;
    }

    xbase::HeaderRec hdr{};

    std::time_t t = std::time(nullptr);
    std::tm lt{};
#if defined(_WIN32)
    localtime_s(&lt, &t);
#else
    lt = *std::localtime(&t);
#endif

    hdr.last_updated[0] = static_cast<std::uint8_t>(lt.tm_year);
    hdr.last_updated[1] = static_cast<std::uint8_t>(lt.tm_mon + 1);
    hdr.last_updated[2] = static_cast<std::uint8_t>(lt.tm_mday);
    hdr.num_of_recs = 0;
    hdr.data_start  = static_cast<std::int16_t>(hdrLen);
    hdr.cpr         = static_cast<std::int16_t>(recLen);

    foxpro_header::stamp_header(
        hdr,
        flavor,
        hasMemo,
        false,
        foxpro_header::CP_DOS_437
    );

    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    if (!out) {
        err = "could not write classic DBF header";
        return false;
    }

    for (const auto& f : fields)
    {
        char name11[11] = {0};
        std::string fn = f.name;
        if (fn.size() > 10) fn.resize(10);
        std::memcpy(name11, fn.c_str(), fn.size());

        out.write(name11, 11);
        out.put((char)f.type);
        put_u32_le(out, 0u);
        out.put((char)f.len);
        out.put((char)f.dec);
        for (int i = 0; i < 14; ++i) out.put(0);

        if (!out) {
            err = "could not write classic field descriptors";
            return false;
        }
    }

    out.put((char)0x0D);
    out.put((char)0x1A);
    out.flush();
    out.close();
    if (!out) {
        err = "could not finalize classic DBF";
        return false;
    }

    if (hasMemo)
    {
        if (flavor == TableFlavor::FOXPRO_26)
            return create_fpt_seed(path, err);
        return create_dbt_seed(path, err);
    }

    return true;
}

#pragma pack(push, 1)
struct VfpHeaderDisk
{
    std::uint8_t  version;
    std::uint8_t  yy;
    std::uint8_t  mm;
    std::uint8_t  dd;
    std::uint32_t num_recs;
    std::uint16_t header_size;
    std::uint16_t record_size;
    std::uint8_t  reserved1[2];
    std::uint8_t  in_transaction;
    std::uint8_t  encrypted;
    std::uint8_t  reserved2[12];
    std::uint8_t  table_flags;
    std::uint8_t  codepage;
    std::uint16_t reserved3;
};

struct VFPFieldRec
{
    char          name[11];
    char          type;
    std::uint32_t offset;
    std::uint8_t  length;
    std::uint8_t  decimals;
    std::uint16_t reserved1;
    std::uint8_t  workarea;
    std::uint16_t reserved2;
    std::uint8_t  flags;
    std::uint8_t  reserved3[8];
};
#pragma pack(pop)

static_assert(sizeof(VfpHeaderDisk) == 32, "VfpHeaderDisk must be 32 bytes");
static_assert(sizeof(VFPFieldRec) == 32, "VFPFieldRec must be 32 bytes");

static bool write_vfp_dbf(const std::string& path,
                          const std::vector<FieldSpec>& fields,
                          std::string& err)
{
    bool hasMemo = false;
    std::uint16_t recLen = 1; // delete flag

    for (const auto& f : fields)
    {
        recLen = static_cast<std::uint16_t>(recLen + f.len);
        if (f.type == 'M') hasMemo = true;
    }

    const std::uint16_t hdrLen =
        static_cast<std::uint16_t>(32 + fields.size() * sizeof(VFPFieldRec) + 1 + 263);

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        err = "could not create VFP DBF file";
        return false;
    }

    xbase::HeaderRec stamp{};
    foxpro_header::stamp_header(
        stamp,
        TableFlavor::VFP,
        hasMemo,
        false,
        foxpro_header::CP_WINDOWS_ANSI
    );

    std::time_t t = std::time(nullptr);
    std::tm lt{};
#if defined(_WIN32)
    localtime_s(&lt, &t);
#else
    lt = *std::localtime(&t);
#endif

    VfpHeaderDisk hdr{};
    hdr.version        = stamp.version;
    hdr.yy             = static_cast<std::uint8_t>(lt.tm_year);
    hdr.mm             = static_cast<std::uint8_t>(lt.tm_mon + 1);
    hdr.dd             = static_cast<std::uint8_t>(lt.tm_mday);
    hdr.num_recs       = 0;
    hdr.header_size    = hdrLen;
    hdr.record_size    = recLen;
    hdr.in_transaction = 0;
    hdr.encrypted      = 0;
    hdr.table_flags    = foxpro_header::table_flags(stamp);
    hdr.codepage       = foxpro_header::code_page(stamp);
    hdr.reserved3      = 0;

    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    if (!out) {
        err = "could not write VFP header";
        return false;
    }

    std::uint32_t offset = 1; // delete flag byte

    for (const auto& f : fields)
    {
        VFPFieldRec vf{};
        std::string fn = f.name;
        if (fn.size() > 10) fn.resize(10);
        std::memcpy(vf.name, fn.c_str(), fn.size());

        vf.type      = f.type;
        vf.offset    = offset;
        vf.length    = f.len;
        vf.decimals  = f.dec;
        vf.reserved1 = 0;
        vf.workarea  = 0;
        vf.reserved2 = 0;
        vf.flags     = 0;
        std::memset(vf.reserved3, 0, sizeof(vf.reserved3));

        out.write(reinterpret_cast<const char*>(&vf), sizeof(vf));
        if (!out) {
            err = "could not write VFP field descriptors";
            return false;
        }

        offset = static_cast<std::uint32_t>(offset + f.len);
    }

    out.put((char)0x0D);

    std::array<char, 263> backlink{};
    out.write(backlink.data(), static_cast<std::streamsize>(backlink.size()));

    out.put((char)0x1A);
    out.flush();
    out.close();
    if (!out) {
        err = "could not finalize VFP DBF";
        return false;
    }

    if (hasMemo)
        return create_fpt_seed(path, err);

    return true;
}

static bool write_x64_dbf(const std::string& path,
                          const std::vector<FieldSpec>& fields,
                          std::string& err)
{
    bool hasMemo = false;
    std::uint16_t recLen = 1; // delete flag

    for (const auto& f : fields)
    {
        recLen = static_cast<std::uint16_t>(recLen + f.len);
        if (f.type == 'M') hasMemo = true;
    }

    const std::uint16_t hdrLen =
        static_cast<std::uint16_t>(
            sizeof(xbase::VfpHeader) +
            sizeof(xbase::LargeHeaderExtension) +
            fields.size() * sizeof(xbase::VfpField) +
            1);

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        err = "could not create X64 DBF file";
        return false;
    }

    std::time_t t = std::time(nullptr);
    std::tm lt{};
#if defined(_WIN32)
    localtime_s(&lt, &t);
#else
    lt = *std::localtime(&t);
#endif

    xbase::VfpHeader hdr{};
    hdr.version        = xbase::DBF_VERSION_64;
    hdr.yy             = static_cast<std::uint8_t>(lt.tm_year);
    hdr.mm             = static_cast<std::uint8_t>(lt.tm_mon + 1);
    hdr.dd             = static_cast<std::uint8_t>(lt.tm_mday);
    hdr.num_recs       = 0;
    hdr.header_size    = hdrLen;
    hdr.record_size    = recLen;
    hdr.in_transaction = 0;
    hdr.encrypted      = 0;
    hdr.table_flags    = hasMemo ? 0x02 : 0x00;
    hdr.codepage       = foxpro_header::CP_WINDOWS_ANSI;
    hdr.reserved3      = 0;

    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    if (!out) {
        err = "could not write X64 header";
        return false;
    }

    xbase::LargeHeaderExtension ext{};
    ext.record_count   = 0;
    ext.data_start_64  = hdrLen;
    ext.record_size_64 = recLen;
    ext.autoq_next     = 1;
    ext.table_flags    = hasMemo ? xbase::DBF64_FLAG_HAS_MEMO : 0;
    ext.reserved32     = 0;
    ext.reserved[0]    = 0;
    ext.reserved[1]    = 0;
    ext.reserved[2]    = 0;

    out.write(reinterpret_cast<const char*>(&ext), sizeof(ext));
    if (!out) {
        err = "could not write X64 extension";
        return false;
    }

    std::uint32_t offset = 1; // delete flag byte

    for (const auto& f : fields)
    {
        xbase::VfpField vf{};
        std::string fn = f.name;
        if (fn.size() > 10) fn.resize(10);
        std::memcpy(vf.name, fn.c_str(), fn.size());

        vf.type         = f.type;
        vf.displacement = offset;
        vf.length       = f.len;
        vf.decimals     = f.dec;
        vf.reserved1    = 0;
        vf.workarea     = 0;
        vf.reserved2    = 0;
        vf.flags        = 0;
        std::memset(vf.reserved3, 0, sizeof(vf.reserved3));

        out.write(reinterpret_cast<const char*>(&vf), sizeof(vf));
        if (!out) {
            err = "could not write X64 field descriptors";
            return false;
        }

        offset = static_cast<std::uint32_t>(offset + f.len);
    }

    out.put((char)0x0D);
    out.flush();
    out.close();
    if (!out) {
        err = "could not finalize X64 DBF";
        return false;
    }

    return true;
}

} // namespace

std::string flavor_name(Flavor flavor)
{
    switch (flavor)
    {
    case Flavor::MSDOS: return "MSDOS/DBASE";
    case Flavor::FOX26: return "FOXPRO 2.6";
    case Flavor::VFP:   return "VFP";
    case Flavor::X64:   return "X64";
    }
    return "UNKNOWN";
}

bool supports_type_now(char code, Flavor flavor) noexcept
{
    const char T = (char)std::toupper((unsigned char)code);

    switch (flavor)
    {
    case Flavor::MSDOS:
    case Flavor::FOX26:
        switch (T)
        {
        case 'C':
        case 'N':
        case 'F':
        case 'D':
        case 'L':
        case 'M':
            return true;
        default:
            return false;
        }

    case Flavor::VFP:
    case Flavor::X64:
        switch (T)
        {
        case 'C':
        case 'N':
        case 'F':
        case 'D':
        case 'L':
        case 'M':
        case 'I':
        case 'B':
        case 'Y':
        case 'T':
            return true;
        default:
            return false;
        }
    }

    return false;
}

bool create_dbf(const std::string& path,
                const std::vector<FieldSpec>& fields,
                Flavor flavor,
                std::string& err)
{
    if (fields.empty()) {
        err = "no fields specified";
        return false;
    }

    switch (flavor)
    {
    case Flavor::VFP:
        return write_vfp_dbf(path, fields, err);
    case Flavor::X64:
        return write_x64_dbf(path, fields, err);
    case Flavor::FOX26:
        return write_classic_dbf(path, fields, TableFlavor::FOXPRO_26, err);
    case Flavor::MSDOS:
    default:
        return write_classic_dbf(path, fields, TableFlavor::MSDOS_DBASE, err);
    }
}

} // namespace xbase::dbf_create
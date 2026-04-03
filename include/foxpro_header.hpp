#pragma once

#include <cstdint>
#include "xbase.hpp"

namespace foxpro_header
{
    // ---------------------------------------------------------------------
    // DBF version bytes
    // ---------------------------------------------------------------------
    constexpr std::uint8_t VER_CLASSIC_NO_MEMO   = 0x03; // dBASE III / classic DBF
    constexpr std::uint8_t VER_CLASSIC_WITH_MEMO = 0x83; // classic DBF + DBT memo
    constexpr std::uint8_t VER_FOX26_WITH_MEMO   = 0xF5; // FoxPro 2.x + memo
    constexpr std::uint8_t VER_VFP_BASE          = 0x30; // VFP
    constexpr std::uint8_t VER_VFP_AUTOINC       = 0x31; // VFP autoinc
    constexpr std::uint8_t VER_VFP_VARCHAR       = 0x32; // VFP varchar/varbinary

    // ---------------------------------------------------------------------
    // HeaderRec::reserved[] mapping for classic 32-byte DBF header
    // xbase::HeaderRec layout:
    //   bytes 12..31 -> reserved[0..19]
    // Therefore:
    //   disk byte 28 -> reserved[16] = table flags
    //   disk byte 29 -> reserved[17] = code page
    // ---------------------------------------------------------------------
    constexpr int RESERVED_INDEX_TABLE_FLAGS = 16;
    constexpr int RESERVED_INDEX_CODE_PAGE   = 17;

    // ---------------------------------------------------------------------
    // Table flags (disk byte 28)
    // ---------------------------------------------------------------------
    constexpr std::uint8_t FLAG_NONE            = 0x00;
    constexpr std::uint8_t FLAG_STRUCTURAL_CDX  = 0x01;
    constexpr std::uint8_t FLAG_MEMO_PRESENT    = 0x02;
    constexpr std::uint8_t FLAG_DATABASE_DBC    = 0x04; // mostly VFP/DBC world

    // ---------------------------------------------------------------------
    // Code page marks (disk byte 29)
    // Keep the first set small and practical.
    // ---------------------------------------------------------------------
    constexpr std::uint8_t CP_DOS_437      = 0x01;
    constexpr std::uint8_t CP_DOS_850      = 0x02;
    constexpr std::uint8_t CP_WINDOWS_ANSI = 0x03;

    // ---------------------------------------------------------------------
    // Table flavor
    // ---------------------------------------------------------------------
    enum class TableFlavor
    {
        MSDOS_DBASE,   // minimal/classic DBF
        FOXPRO_26,     // FoxPro 2.6-style header stamping
        VFP            // Visual FoxPro-style version byte stamping
    };

    // ---------------------------------------------------------------------
    // Header accessors/setters using existing packed HeaderRec
    // ---------------------------------------------------------------------
    inline std::uint8_t table_flags(const xbase::HeaderRec& hdr) noexcept
    {
        return hdr.reserved[RESERVED_INDEX_TABLE_FLAGS];
    }

    inline std::uint8_t code_page(const xbase::HeaderRec& hdr) noexcept
    {
        return hdr.reserved[RESERVED_INDEX_CODE_PAGE];
    }

    inline void set_table_flags(xbase::HeaderRec& hdr, std::uint8_t flags) noexcept
    {
        hdr.reserved[RESERVED_INDEX_TABLE_FLAGS] = flags;
    }

    inline void set_code_page(xbase::HeaderRec& hdr, std::uint8_t cp) noexcept
    {
        hdr.reserved[RESERVED_INDEX_CODE_PAGE] = cp;
    }

    inline void add_table_flag(xbase::HeaderRec& hdr, std::uint8_t flag) noexcept
    {
        hdr.reserved[RESERVED_INDEX_TABLE_FLAGS] =
            static_cast<std::uint8_t>(hdr.reserved[RESERVED_INDEX_TABLE_FLAGS] | flag);
    }

    inline void clear_table_flags(xbase::HeaderRec& hdr) noexcept
    {
        hdr.reserved[RESERVED_INDEX_TABLE_FLAGS] = 0;
    }

    // ---------------------------------------------------------------------
    // Pick a version byte from flavor + memo presence
    // ---------------------------------------------------------------------
    inline std::uint8_t choose_version(TableFlavor flavor, bool hasMemo) noexcept
    {
        switch (flavor)
        {
        case TableFlavor::MSDOS_DBASE:
            return hasMemo ? VER_CLASSIC_WITH_MEMO : VER_CLASSIC_NO_MEMO;

        case TableFlavor::FOXPRO_26:
            // FoxPro 2.6 commonly uses 0x03 without memo, 0xF5 with memo.
            return hasMemo ? VER_FOX26_WITH_MEMO : VER_CLASSIC_NO_MEMO;

        case TableFlavor::VFP:
            // First cut: VFP base table. If you later add true VFP field
            // descriptors/backlink logic, this version byte is already ready.
            return VER_VFP_BASE;
        }

        return hasMemo ? VER_CLASSIC_WITH_MEMO : VER_CLASSIC_NO_MEMO;
    }

    // ---------------------------------------------------------------------
    // Stamp common metadata into HeaderRec
    // ---------------------------------------------------------------------
    inline void stamp_header(xbase::HeaderRec& hdr,
                             TableFlavor flavor,
                             bool hasMemo,
                             bool hasStructuralCdx = false,
                             std::uint8_t cp = CP_DOS_437) noexcept
    {
        hdr.version = choose_version(flavor, hasMemo);

        clear_table_flags(hdr);

        if (hasMemo)
            add_table_flag(hdr, FLAG_MEMO_PRESENT);

        if (hasStructuralCdx)
            add_table_flag(hdr, FLAG_STRUCTURAL_CDX);

        set_code_page(hdr, cp);
    }
}
// datatype_index.hpp
// Central type catalog / mapping index for DotTalk++
//
// Purpose:
//   - Provide one authoritative place for DBF/Fox/VFP/SQL type knowledge
//   - Keep storage type, semantic type, and external mapping separate
//   - Allow gradual integration into CREATE / USE / STRUCT / importsql / tuptalk
//
// Notes:
//   - Header-only by design
//   - Declarative first; runtime policy can be layered on later
//   - Conservative defaults: unsupported mappings are explicit

#pragma once

#include <array>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string_view>

namespace dottalk::types {

// -----------------------------------------------------------------------------
// Engine/file families
// -----------------------------------------------------------------------------
enum class EngineFormat : std::uint8_t {
    Unknown = 0,
    MSDOS_DBASE,
    FOX26,
    VFP,
    SQL,
    TUPTALK
};

// -----------------------------------------------------------------------------
// Physical storage family
// -----------------------------------------------------------------------------
enum class StorageFamily : std::uint8_t {
    Unknown = 0,
    FixedText,
    FixedNumericAscii,
    DateYYYYMMDD,
    LogicalAscii,
    MemoPointer,
    Integer32Binary,
    Currency64Binary,
    DateTimeBinary,
    DoubleBinary,
    VarChar,
    VarBinary,
    Blob,
    General,
    Picture
};

// -----------------------------------------------------------------------------
// Logical semantic type
// -----------------------------------------------------------------------------
enum class SemanticType : std::uint8_t {
    Unknown = 0,
    Character,
    Numeric,
    Integer,
    Decimal,
    Date,
    DateTime,
    Logical,
    Memo,
    Currency,
    DoubleFloat,
    Binary,
    Blob,
    General,
    Picture,
    AutoIncrement
};

// -----------------------------------------------------------------------------
// Support state
// -----------------------------------------------------------------------------
enum class SupportLevel : std::uint8_t {
    None = 0,
    Planned,
    ParseOnly,
    ReadWrite
};

// -----------------------------------------------------------------------------
// Per-type capability flags
// -----------------------------------------------------------------------------
enum TypeFlags : std::uint32_t {
    TF_NONE            = 0,
    TF_HAS_WIDTH       = 1u << 0,
    TF_HAS_DECIMALS    = 1u << 1,
    TF_IS_BINARY       = 1u << 2,
    TF_IS_MEMO         = 1u << 3,
    TF_IS_VARIABLE     = 1u << 4,
    TF_IS_NUMERIC      = 1u << 5,
    TF_IS_TEXTUAL      = 1u << 6,
    TF_IS_TEMPORAL     = 1u << 7,
    TF_CAN_INDEX       = 1u << 8,
    TF_SQL_FRIENDLY    = 1u << 9,
    TF_TUPLE_FRIENDLY  = 1u << 10,
    TF_VFP_ONLY        = 1u << 11,
    TF_CLASSIC_ONLY    = 1u << 12
};

constexpr inline bool has_flag(std::uint32_t value, std::uint32_t flag) noexcept {
    return (value & flag) != 0;
}

// -----------------------------------------------------------------------------
// Canonical type record
// -----------------------------------------------------------------------------
struct TypeInfo {
    char            dbf_code;         // physical DBF/Fox/VFP type code; 0 if N/A
    std::string_view short_name;      // "C", "N", "D", "M", "I", ...
    std::string_view display_name;    // "Character", "Numeric", ...
    StorageFamily   storage_family;
    SemanticType    semantic_type;
    std::uint32_t   flags;

    SupportLevel    msdos_support;
    SupportLevel    fox26_support;
    SupportLevel    vfp_support;
    SupportLevel    sql_support;
    SupportLevel    tuptalk_support;

    std::string_view default_sql_type;
    std::string_view notes;
};

// -----------------------------------------------------------------------------
// Canonical index
//
// Current scope:
//   - conservative classic xBase / FoxPro / early VFP coverage
//   - SQL/TUPTALK columns describe intended mapping readiness
//   - extend here first before spreading type logic elsewhere
// -----------------------------------------------------------------------------
constexpr inline std::array<TypeInfo, 14> kTypeIndex{{
    {
        'C', "C", "Character",
        StorageFamily::FixedText, SemanticType::Character,
        TF_HAS_WIDTH | TF_IS_TEXTUAL | TF_CAN_INDEX | TF_SQL_FRIENDLY | TF_TUPLE_FRIENDLY,
        SupportLevel::ReadWrite, SupportLevel::ReadWrite, SupportLevel::ReadWrite,
        SupportLevel::ReadWrite, SupportLevel::ReadWrite,
        "VARCHAR",
        "Classic fixed-length character field."
    },
    {
        'N', "N", "Numeric",
        StorageFamily::FixedNumericAscii, SemanticType::Numeric,
        TF_HAS_WIDTH | TF_HAS_DECIMALS | TF_IS_NUMERIC | TF_CAN_INDEX | TF_SQL_FRIENDLY | TF_TUPLE_FRIENDLY,
        SupportLevel::ReadWrite, SupportLevel::ReadWrite, SupportLevel::ReadWrite,
        SupportLevel::ReadWrite, SupportLevel::ReadWrite,
        "DECIMAL",
        "ASCII numeric storage; decimals determine integer vs decimal usage."
    },
    {
        'D', "D", "Date",
        StorageFamily::DateYYYYMMDD, SemanticType::Date,
        TF_IS_TEMPORAL | TF_CAN_INDEX | TF_SQL_FRIENDLY | TF_TUPLE_FRIENDLY,
        SupportLevel::ReadWrite, SupportLevel::ReadWrite, SupportLevel::ReadWrite,
        SupportLevel::ReadWrite, SupportLevel::ReadWrite,
        "DATE",
        "Stored as YYYYMMDD in DBF family formats."
    },
    {
        'L', "L", "Logical",
        StorageFamily::LogicalAscii, SemanticType::Logical,
        TF_CAN_INDEX | TF_SQL_FRIENDLY | TF_TUPLE_FRIENDLY,
        SupportLevel::ReadWrite, SupportLevel::ReadWrite, SupportLevel::ReadWrite,
        SupportLevel::ReadWrite, SupportLevel::ReadWrite,
        "BOOLEAN",
        "Logical true/false/? style field."
    },
    {
        'M', "M", "Memo",
        StorageFamily::MemoPointer, SemanticType::Memo,
        TF_IS_MEMO | TF_IS_TEXTUAL | TF_SQL_FRIENDLY | TF_TUPLE_FRIENDLY,
        SupportLevel::ReadWrite, SupportLevel::ReadWrite, SupportLevel::ReadWrite,
        SupportLevel::ReadWrite, SupportLevel::ReadWrite,
        "TEXT",
        "Memo pointer into DBT/FPT sidecar."
    },
    {
        'F', "F", "Float",
        StorageFamily::FixedNumericAscii, SemanticType::DoubleFloat,
        TF_HAS_WIDTH | TF_HAS_DECIMALS | TF_IS_NUMERIC | TF_SQL_FRIENDLY | TF_TUPLE_FRIENDLY,
        SupportLevel::Planned, SupportLevel::Planned, SupportLevel::Planned,
        SupportLevel::ReadWrite, SupportLevel::Planned,
        "FLOAT",
        "Rare in classic xBase usage; treat carefully."
    },
    {
        'I', "I", "Integer",
        StorageFamily::Integer32Binary, SemanticType::Integer,
        TF_IS_NUMERIC | TF_SQL_FRIENDLY | TF_TUPLE_FRIENDLY | TF_VFP_ONLY,
        SupportLevel::None, SupportLevel::None, SupportLevel::Planned,
        SupportLevel::ReadWrite, SupportLevel::Planned,
        "INTEGER",
        "Visual FoxPro 32-bit integer."
    },
    {
        'Y', "Y", "Currency",
        StorageFamily::Currency64Binary, SemanticType::Currency,
        TF_IS_NUMERIC | TF_SQL_FRIENDLY | TF_TUPLE_FRIENDLY | TF_VFP_ONLY,
        SupportLevel::None, SupportLevel::None, SupportLevel::Planned,
        SupportLevel::ReadWrite, SupportLevel::Planned,
        "DECIMAL(19,4)",
        "Visual FoxPro scaled 64-bit currency."
    },
    {
        'T', "T", "DateTime",
        StorageFamily::DateTimeBinary, SemanticType::DateTime,
        TF_IS_TEMPORAL | TF_SQL_FRIENDLY | TF_TUPLE_FRIENDLY | TF_VFP_ONLY,
        SupportLevel::None, SupportLevel::None, SupportLevel::Planned,
        SupportLevel::ReadWrite, SupportLevel::Planned,
        "DATETIME",
        "Visual FoxPro datetime."
    },
    {
        'B', "B", "Double",
        StorageFamily::DoubleBinary, SemanticType::DoubleFloat,
        TF_IS_NUMERIC | TF_SQL_FRIENDLY | TF_TUPLE_FRIENDLY | TF_VFP_ONLY,
        SupportLevel::None, SupportLevel::None, SupportLevel::Planned,
        SupportLevel::ReadWrite, SupportLevel::Planned,
        "DOUBLE",
        "Visual FoxPro binary double."
    },
    {
        'V', "V", "VarChar",
        StorageFamily::VarChar, SemanticType::Character,
        TF_HAS_WIDTH | TF_IS_TEXTUAL | TF_IS_VARIABLE | TF_SQL_FRIENDLY | TF_TUPLE_FRIENDLY | TF_VFP_ONLY,
        SupportLevel::None, SupportLevel::None, SupportLevel::Planned,
        SupportLevel::ReadWrite, SupportLevel::Planned,
        "VARCHAR",
        "Visual FoxPro varchar."
    },
    {
        'Q', "Q", "VarBinary",
        StorageFamily::VarBinary, SemanticType::Binary,
        TF_HAS_WIDTH | TF_IS_BINARY | TF_IS_VARIABLE | TF_SQL_FRIENDLY | TF_VFP_ONLY,
        SupportLevel::None, SupportLevel::None, SupportLevel::Planned,
        SupportLevel::ReadWrite, SupportLevel::Planned,
        "VARBINARY",
        "Visual FoxPro varbinary."
    },
    {
        'W', "W", "Blob",
        StorageFamily::Blob, SemanticType::Blob,
        TF_IS_BINARY | TF_IS_MEMO | TF_SQL_FRIENDLY | TF_VFP_ONLY,
        SupportLevel::None, SupportLevel::None, SupportLevel::Planned,
        SupportLevel::ReadWrite, SupportLevel::Planned,
        "BLOB",
        "Visual FoxPro blob/generalized binary memo-style storage."
    },
    {
        'G', "G", "General",
        StorageFamily::General, SemanticType::General,
        TF_IS_MEMO | TF_VFP_ONLY,
        SupportLevel::None, SupportLevel::None, SupportLevel::Planned,
        SupportLevel::ParseOnly, SupportLevel::Planned,
        "BLOB",
        "Visual FoxPro general field; often OLE/object-like payload."
    }
}};

// -----------------------------------------------------------------------------
// Lookup helpers
// -----------------------------------------------------------------------------
constexpr inline char up_char(char c) noexcept {
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}

constexpr inline const TypeInfo* find_by_code(char dbf_code) noexcept {
    const char key = up_char(dbf_code);
    for (const auto& t : kTypeIndex) {
        if (t.dbf_code == key) return &t;
    }
    return nullptr;
}

constexpr inline const TypeInfo* find_by_semantic(SemanticType st) noexcept {
    for (const auto& t : kTypeIndex) {
        if (t.semantic_type == st) return &t;
    }
    return nullptr;
}

inline const TypeInfo* find_by_sql_name(std::string_view sql_name) noexcept {
    auto upper_eq = [](std::string_view a, std::string_view b) -> bool {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) {
            const unsigned char ca = static_cast<unsigned char>(a[i]);
            const unsigned char cb = static_cast<unsigned char>(b[i]);
            if (std::toupper(ca) != std::toupper(cb)) return false;
        }
        return true;
    };

    for (const auto& t : kTypeIndex) {
        if (upper_eq(t.default_sql_type, sql_name)) return &t;
    }
    return nullptr;
}

// -----------------------------------------------------------------------------
// Support matrix helpers
// -----------------------------------------------------------------------------
constexpr inline SupportLevel support_for(const TypeInfo& t, EngineFormat fmt) noexcept {
    switch (fmt) {
        case EngineFormat::MSDOS_DBASE: return t.msdos_support;
        case EngineFormat::FOX26:       return t.fox26_support;
        case EngineFormat::VFP:         return t.vfp_support;
        case EngineFormat::SQL:         return t.sql_support;
        case EngineFormat::TUPTALK:     return t.tuptalk_support;
        default:                        return SupportLevel::None;
    }
}

constexpr inline bool is_supported(const TypeInfo& t, EngineFormat fmt) noexcept {
    return support_for(t, fmt) != SupportLevel::None;
}

constexpr inline bool is_readwrite(const TypeInfo& t, EngineFormat fmt) noexcept {
    return support_for(t, fmt) == SupportLevel::ReadWrite;
}

// -----------------------------------------------------------------------------
// Derived semantic helpers
// -----------------------------------------------------------------------------
constexpr inline bool is_numeric_semantic(SemanticType st) noexcept {
    return st == SemanticType::Numeric ||
           st == SemanticType::Integer ||
           st == SemanticType::Decimal ||
           st == SemanticType::Currency ||
           st == SemanticType::DoubleFloat ||
           st == SemanticType::AutoIncrement;
}

constexpr inline bool is_text_semantic(SemanticType st) noexcept {
    return st == SemanticType::Character || st == SemanticType::Memo;
}

constexpr inline bool is_temporal_semantic(SemanticType st) noexcept {
    return st == SemanticType::Date || st == SemanticType::DateTime;
}

constexpr inline SemanticType derive_semantic_type(char dbf_code,
                                                   std::uint8_t width,
                                                   std::uint8_t decimals) noexcept
{
    const TypeInfo* t = find_by_code(dbf_code);
    if (!t) return SemanticType::Unknown;

    if (t->dbf_code == 'N') {
        if (decimals == 0) return SemanticType::Integer;
        return SemanticType::Decimal;
    }

    return t->semantic_type;
}

// -----------------------------------------------------------------------------
// SQL mapping helper
// Conservative, string-view only. Precise formatting can be layered later.
// -----------------------------------------------------------------------------
constexpr inline std::string_view sql_type_for(char dbf_code,
                                               std::uint8_t width,
                                               std::uint8_t decimals) noexcept
{
    (void)width;

    const TypeInfo* t = find_by_code(dbf_code);
    if (!t) return "UNKNOWN";

    if (up_char(dbf_code) == 'N') {
        return (decimals == 0) ? "INTEGER" : "DECIMAL";
    }

    return t->default_sql_type;
}

// -----------------------------------------------------------------------------
// CREATE validation helper
// Useful for CREATE / schema import / conversion planning
// -----------------------------------------------------------------------------
struct ValidationResult {
    bool            ok;
    const TypeInfo* type;
    const char*     reason;
};

constexpr inline ValidationResult validate_type_for_format(char dbf_code,
                                                           EngineFormat fmt) noexcept
{
    const TypeInfo* t = find_by_code(dbf_code);
    if (!t) {
        return { false, nullptr, "Unknown field type code." };
    }

    if (!is_supported(*t, fmt)) {
        return { false, t, "Field type is not supported for this engine format." };
    }

    return { true, t, nullptr };
}

// -----------------------------------------------------------------------------
// Open hook surface
// These are intentionally small and non-invasive.
// The app can start using them without a runtime registry object.
// -----------------------------------------------------------------------------
struct ResolvedFieldType {
    char            dbf_code;
    SemanticType    semantic_type;
    StorageFamily   storage_family;
    std::uint8_t    width;
    std::uint8_t    decimals;
    const TypeInfo* info;
};

constexpr inline ResolvedFieldType resolve_field_type(char dbf_code,
                                                      std::uint8_t width,
                                                      std::uint8_t decimals) noexcept
{
    const TypeInfo* t = find_by_code(dbf_code);
    return {
        up_char(dbf_code),
        derive_semantic_type(dbf_code, width, decimals),
        t ? t->storage_family : StorageFamily::Unknown,
        width,
        decimals,
        t
    };
}

// -----------------------------------------------------------------------------
// Suggested integration points
//
// CREATE:
//   validate_type_for_format(type, format)
//
// USE/open:
//   resolve_field_type(field.type, field.length, field.decimals)
//
// STRUCT/FIELDS:
//   info->display_name / semantic_type
//
// importsql:
//   find_by_sql_name(...) or sql_type_for(...)
//
// tuptalk:
//   resolve_field_type(...).semantic_type
// -----------------------------------------------------------------------------

} // namespace dottalk::types
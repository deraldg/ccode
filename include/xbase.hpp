// xbase.hpp
// Updated with public setters for VFP compatibility, encapsulation,
// 64-bit runtime record tracking, and runtime area-kind detection.

#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <optional>
#include <limits>

// Forward-declare to avoid heavy include & circular deps in the header.
// Define DbArea's destructor AND move ops in a .cpp that includes "xindex/index_manager.hpp".
namespace xindex { class IndexManager; }

namespace xbase {

// ---- Constants -------------------------------------------------------------
constexpr int         MAX_FIELDS         = 128;
constexpr int         MAX_INDEX          = 5;
constexpr int         MAX_AREA           = 256;
constexpr char        IS_DELETED         = '*';
constexpr char        NOT_DELETED        = ' ';
constexpr uint8_t     HEADER_TERM_BYTE   = 0x0D;

// ---- Runtime Area Kind / Capability ---------------------------------------
enum class AreaKind : uint8_t {
    Unknown = 0,
    V32,
    V64,
    V128,
    Tup
};

enum class AreaCapability : uint64_t {
    None        = 0,
    ReadRows    = 1ull << 0,
    WriteRows   = 1ull << 1,
    DeleteRows  = 1ull << 2,
    Seek        = 1ull << 3,
    Filter      = 1ull << 4,
    Order       = 1ull << 5,
    Relations   = 1ull << 6,
    Memo        = 1ull << 7,
    TupleOps    = 1ull << 8
};

// Centralized version-byte -> runtime kind mapping.
// This keeps flavor detection in one place and lets loaders simply stamp DbArea.
inline AreaKind detect_area_kind_from_version(std::uint8_t ver) noexcept
{
    switch (ver) {
        case 0x03: // ClassicNoMemo
        case 0x83: // ClassicWithMemo
        case 0xF5: // Fox26Memo
            return AreaKind::V32;

        case 0x30: // VfpBase
        case 0x31: // VfpAutoInc
        case 0x32: // VfpVar
        case 0x64: // xbase_64 dialect
            return AreaKind::V64;

        default:
            return AreaKind::Unknown;
    }
}

// ---- Memo field storage contract ------------------------------------------
constexpr std::uint8_t LEGACY_MEMO_FIELD_LEN = 10; // pre-x64: plain fixed-width text slot
constexpr std::uint8_t X64_MEMO_FIELD_LEN    = 8;  // x64: uint64 object-id slot

// ---- On-disk structures (packed) ------------------------------------------
#pragma pack(push, 1)
struct HeaderRec {
    uint8_t   version;
    uint8_t   last_updated[3];
    int32_t   num_of_recs;
    int16_t   data_start;
    int16_t   cpr;              // characters per record (record length)
    uint8_t   reserved[20];
};

struct FieldRec {
    char      field_name[11];
    char      field_type;
    uint32_t  field_data_address; // present in some variants
    uint8_t   field_length;
    uint8_t   decimal_places;
    uint8_t   reserved[14];
};
#pragma pack(pop)

// ---- In-memory field metadata ---------------------------------------------
struct FieldDef {
    std::string name;
    char        type{};       // 'C','N','D','L', etc.
    uint8_t     length{};     // total bytes
    uint8_t     decimals{};   // for 'N'
};

namespace {
    static inline void clear() {
    #ifdef _WIN32
        std::system("cls");
    #else
        std::fputs("\x1b[2J\x1b[H", stdout);
        std::fflush(stdout);
    #endif
    }
}

// ---- DbArea ---------------------------------------------------------------
class DbArea {
public:
    // ---- Types ------------------------------------------------------------
    enum class MemoKind { NONE, FPT, DBT };

    // ---- Lifecycle --------------------------------------------------------
    DbArea();
    ~DbArea();                                   // defined out-of-line in .cpp

    DbArea(const DbArea&)            = delete;
    DbArea& operator=(const DbArea&) = delete;

    // Move ops are defined out-of-line (MSVC-safe with forward-declared IndexManager).
    DbArea(DbArea&&);
    DbArea& operator=(DbArea&&);

    // ---- Open / Close -----------------------------------------------------
    // Resolver provides absolute DBF path
    void open(const std::string& abs_filename);
    void close();

    // ---- State ------------------------------------------------------------
    bool isOpen() const noexcept { return _fp.is_open(); }
    bool isDeleted() const;

    // ---- Runtime kind / capability ---------------------------------------
    AreaKind kind() const noexcept { return _kind; }
    void setKind(AreaKind k) noexcept { _kind = k; }
    bool supports(AreaCapability cap) const noexcept;

    // ---- Navigation -------------------------------------------------------
    bool gotoRec(int32_t recno);
    bool top();
    bool bottom();
    bool skip(int delta);

    // ---- Record I/O -------------------------------------------------------
    bool readCurrent();
    bool writeCurrent();
    bool appendBlank();
    bool deleteCurrent();

    // ---- Record size ------------------------------------------------------
    int  recLength() const noexcept { return _hdr.cpr; }
    int  recordLength() const noexcept;
    int  cpr() const noexcept { return recLength(); }

    // ---- Runtime truth (CANONICAL) ----------------------------------------
    const std::string& filename() const noexcept { return _dbf_abs_path; }
    const std::string& dbfDir() const noexcept { return _dbf_dir; }
    const std::string& dbfBasename() const noexcept { return _dbf_basename; }
    const std::string& logicalName() const noexcept { return _logical_name; }

    // ---- Index manager (per-area) -----------------------------------------
    xindex::IndexManager& indexManager();
    const xindex::IndexManager* indexManagerPtr() const noexcept;

    // ---- Memo sidecar facts (co-located with DBF) -------------------------
    const std::string& memoPath() const noexcept { return _memo_abs_path; }
    MemoKind           memoKind() const noexcept { return _memo_kind; }

    // ---- Field access (1-based) -------------------------------------------
    const std::vector<FieldDef>& fields() const { return _fields; }
    std::string get(int idx) const;
    bool        set(int idx, const std::string& val);

    // ---- Info -------------------------------------------------------------
    int32_t recno() const noexcept {
        return (_crn64 > static_cast<uint64_t>(std::numeric_limits<int32_t>::max()))
            ? std::numeric_limits<int32_t>::max()
            : static_cast<int32_t>(_crn64);
    }
    int32_t recCount() const noexcept {
        return (_rec_count64 > static_cast<uint64_t>(std::numeric_limits<int32_t>::max()))
            ? std::numeric_limits<int32_t>::max()
            : static_cast<int32_t>(_rec_count64);
    }

    uint64_t recno64() const noexcept    { return _crn64; }
    uint64_t recCount64() const noexcept { return _rec_count64; }

    bool bof() const noexcept { return _crn64 == 0; }
    bool eof() const noexcept { return _rec_count64 == 0 || _crn64 > _rec_count64; }
    int  fieldCount() const { return static_cast<int>(_fields.size()); }

    // ---- Legacy compatibility --------------------------------------------
    std::string name() const { return _logical_name; }
    void        setFilename(std::string path);

    // ---- Internal lifecycle helpers --------------------------------------
    void _compute_paths_and_names_(const std::string& abs_dbf_path);
    void _clear_paths_and_names_() noexcept;

    // ---- New setters for OOP encapsulation and VFP loader compatibility ---
    void clearFields() noexcept {
        _fields.clear();
        _rawFields.clear();
    }
    void addField(FieldDef fd) {
        _fields.push_back(std::move(fd));
    }
    void addRawField(FieldRec fr) {
        _rawFields.push_back(std::move(fr));
    }
    void setVersionByte(uint8_t ver) noexcept {
        _dbf_version_byte = ver;
        _hdr.version = ver;
    }
    uint8_t versionByte() const noexcept {
        return _dbf_version_byte;
    }
    void setHeader(const HeaderRec& hdr) noexcept {
        _hdr = hdr;
        _dbf_version_byte = hdr.version;
        _rec_count64 = (hdr.num_of_recs < 0)
            ? 0u
            : static_cast<uint64_t>(hdr.num_of_recs);
    }
    void setRecordCount(int32_t n) noexcept {
        _hdr.num_of_recs = n;
        _rec_count64 = (n < 0) ? 0u : static_cast<uint64_t>(n);
    }
    void setRecordCount64(uint64_t n) noexcept {
        _rec_count64 = n;
        _hdr.num_of_recs = (n > static_cast<uint64_t>(std::numeric_limits<int32_t>::max()))
            ? std::numeric_limits<int32_t>::max()
            : static_cast<int32_t>(n);
    }
    void setRecno64(uint64_t n) noexcept {
        _crn64 = n;
        _crn = (n > static_cast<uint64_t>(std::numeric_limits<int32_t>::max()))
            ? std::numeric_limits<int32_t>::max()
            : static_cast<int32_t>(n);
    }
    void setDataStart(int16_t start) noexcept {
        _hdr.data_start = start;
    }
    void setRecordLength(int16_t len) noexcept {
        _hdr.cpr = len;
    }
    void setLastUpdated(uint8_t yy, uint8_t mm, uint8_t dd) noexcept {
        _hdr.last_updated[0] = yy;
        _hdr.last_updated[1] = mm;
        _hdr.last_updated[2] = dd;
    }

    // ---- 64-bit DBF compatibility additions -------------------------------
    void setAutoQNext64(uint64_t v) noexcept {
        _autoq_next64 = v;
    }
    uint64_t autoQNext64() const noexcept {
        return _autoq_next64;
    }

    void setTableFlags(uint32_t v) noexcept {
        _table_flags = v;
    }
    uint32_t tableFlags() const noexcept {
        return _table_flags;
    }

private:
    // ===== Storage =========================================================
    std::fstream _fp;
    HeaderRec    _hdr{};

    // ===== Schema & buffers ===============================================
    std::vector<FieldDef>  _fields;
    std::vector<FieldRec>  _rawFields;
    std::vector<char>      _recbuf;

    // Current record values (1-based indexing: slot 0 unused)
    std::vector<std::string> _fd;
    // Snapshot used by indexing to compute old/new keys on update
    std::vector<std::string> _fd_snapshot;

    // ===== Cursor state ====================================================
    int32_t  _crn{0};
    uint64_t _crn64{0};
    uint64_t _rec_count64{0};
    char     _del{NOT_DELETED};

    // ===== Runtime kind ====================================================
    AreaKind _kind{AreaKind::Unknown};

    // ===== Per-area managers ==============================================
    std::unique_ptr<xindex::IndexManager> _idx;

    // ===== Runtime descriptors (CANONICAL) ================================
    std::string _dbf_abs_path;
    std::string _dbf_dir;
    std::string _dbf_basename;
    std::string _dbf_ext;
    std::string _logical_name;

    // Memo sidecar
    std::string _memo_abs_path;
    MemoKind    _memo_kind{MemoKind::NONE};

    // ===== Legacy storage (DEPRECATED, mapped internally) =================
    std::string _db_name;
    std::string _filename;

    // ===== VFP compatibility additions =====================================
    uint8_t     _dbf_version_byte{0x03};

    // ===== 64-bit DBF compatibility additions ==============================
    uint64_t    _autoq_next64{0};
    uint32_t    _table_flags{0};

    // ===== Internals =======================================================
    void        readHeader();
    void        readFields();
    bool        loadFieldsFromBuffer();
    void        storeFieldsToBuffer();
    static std::string rtrim(std::string s);

    // Index helpers
    int         findFieldCI(const std::string& name) const;
    int         firstCharField() const;
    std::vector<uint8_t> encodeKeyFrom(const std::vector<std::string>& vals) const;
    std::vector<uint8_t> currentKey()  const { return encodeKeyFrom(_fd); }
    std::vector<uint8_t> snapshotKey() const { return encodeKeyFrom(_fd_snapshot); }
};

// ---- Engine wrapper --------------------------------------------------------
class XBaseEngine {
public:
    XBaseEngine();
    DbArea& area(int idx) {
        if (idx < 0 || idx >= MAX_AREA) throw std::out_of_range("area");
        return *_areas[idx];
    }
    void selectArea(int idx) {
        if (idx < 0 || idx >= MAX_AREA) throw std::out_of_range("area");
        _current = idx;
    }
    int currentArea() const { return _current; }

private:
    std::array<std::unique_ptr<DbArea>, MAX_AREA> _areas;
    int _current{0};
};

// Helpers
std::string dbNameWithExt(std::string s); // ensure .dbf

} // namespace xbase

using DbArea = xbase::DbArea;
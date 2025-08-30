#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <optional>

// Forward-declare to avoid heavy include & circular deps in the header.
// Define DbArea's destructor in a .cpp that includes "xindex/index_manager.hpp".
namespace xindex { class IndexManager; }

namespace xbase {

// ---- Constants -------------------------------------------------------------
constexpr int     MAX_FIELDS         = 128;
constexpr int     MAX_INDEX          = 5;
constexpr int     MAX_AREA           = 10;
constexpr char    IS_DELETED         = '*';
constexpr char    NOT_DELETED        = ' ';
constexpr uint8_t HEADER_TERM_BYTE   = 0x0D;

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

// ---- DbArea ----------------------------------------------------------------
class DbArea {
public:
    DbArea();
    ~DbArea();                                   // defined out-of-line in a .cpp

    // Non-copyable, movable
    DbArea(const DbArea&)            = delete;
    DbArea& operator=(const DbArea&) = delete;
    DbArea(DbArea&&)                 = default;
    DbArea& operator=(DbArea&&)      = default;

    // Open/close
    void open(const std::string& filename);
    void close();
    bool isOpen() const noexcept { return static_cast<bool>(_fp); }
    bool isDeleted() const;

    // Navigation
    bool gotoRec(int32_t recno);
    bool top();
    bool bottom();
    bool skip(int delta);

    // Record I/O
    bool readCurrent();
    bool writeCurrent();
    bool appendBlank();
    bool deleteCurrent();

    // ---- Accessors expected by the rest of the app -------------------------
    int         recordLength() const noexcept;    // bytes per record (from _hdr.cpr)
    std::string filename() const;                 // full path or base name
    void        setFilename(std::string path);    // helper to set on open/create

    // Field access (1-based)
    const std::vector<FieldDef>& fields() const { return _fields; }
    std::string get(int idx) const;              // 1-based
    bool        set(int idx, const std::string& val); // 1-based

    // Info
    int32_t     recno()      const { return _crn; }
    int32_t     recCount()   const { return _hdr.num_of_recs; }
    int         fieldCount() const { return static_cast<int>(_fields.size()); }
    int         cpr()        const { return _hdr.cpr; }   // same as recordLength()
    std::string name()       const { return _db_name; }

private:
    // Storage
    std::fstream _fp;
    std::string  _db_name;         // base name (e.g., "CUSTOMER")
    std::string  _filename;        // full path or provided name
    HeaderRec    _hdr{};           // header as read from the file

    // Schema & buffers
    std::vector<FieldDef>  _fields;      // normalized field defs
    std::vector<FieldRec>  _rawFields;   // on-disk field descriptors
    std::vector<char>      _recbuf;      // raw record buffer

    // Current record values (1-based indexing: slot 0 unused)
    std::vector<std::string> _fd;
    // Snapshot used by indexing to compute old/new keys on update
    std::vector<std::string> _fd_snapshot;

    // Cursor state
    int32_t _crn{0};               // current record number (1-based; 0 = BOF)
    char    _del{NOT_DELETED};     // deletion flag for current record

    // Per-area index manager (incomplete type here; owned & destroyed in .cpp)
    std::unique_ptr<xindex::IndexManager> _idx;

    // Internals
    void        readHeader();
    void        readFields();
    bool        loadFieldsFromBuffer();
    void        storeFieldsToBuffer();
    static std::string rtrim(std::string s);

    // Index helpers
    int         findFieldCI(const std::string& name) const; // returns 1-based idx or 0
    int         firstCharField() const;                      // 1-based idx or 0
    std::vector<uint8_t> encodeKeyFrom(const std::vector<std::string>& vals) const;
    std::vector<uint8_t> currentKey()  const { return encodeKeyFrom(_fd); }
    std::vector<uint8_t> snapshotKey() const { return encodeKeyFrom(_fd_snapshot); }
};

// ---- Engine wrapper --------------------------------------------------------
class XBaseEngine {
public:
    XBaseEngine();
    DbArea& area(int idx) { if (idx < 0 || idx >= MAX_AREA) throw std::out_of_range("area"); return *_areas[idx]; }
    void selectArea(int idx) { if (idx < 0 || idx >= MAX_AREA) throw std::out_of_range("area"); _current = idx; }
    int  currentArea() const { return _current; }
private:
    std::array<std::unique_ptr<DbArea>, MAX_AREA> _areas;
    int _current{0};
};

// Helpers
std::string dbNameWithExt(std::string s); // ensure .dbf

} // namespace xbase

// ---- Legacy alias (global scope) -------------------------------------------
// Old code refers to ::DbArea; keep that working without a duplicate class.
using DbArea = xbase::DbArea;

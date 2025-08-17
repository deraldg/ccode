#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <optional>

// [INDEX PATCH]
#include "xindex/index_manager.hpp"


namespace xbase {

constexpr int MAX_FIELDS = 128;
constexpr int MAX_INDEX  = 5;
constexpr int MAX_AREA   = 10;
constexpr char IS_DELETED = '*';
constexpr char NOT_DELETED = ' ';
constexpr uint8_t HEADER_TERM_BYTE = 0x0D;

#pragma pack(push, 1)
struct HeaderRec {
    uint8_t   version;
    uint8_t   last_updated[3];
    int32_t   num_of_recs;
    int16_t   data_start;
    int16_t   cpr; // chars per record
    uint8_t   reserved[20];
};

struct FieldRec {
    char      field_name[11];
    char      field_type;
    uint32_t  field_data_address; // not used by dBASE III but appears in some variants
    uint8_t   field_length;
    uint8_t   decimal_places;
    uint8_t   reserved[14];
};
#pragma pack(pop)

struct FieldDef {
    std::string name;
    char type{};
    uint8_t length{};
    uint8_t decimals{};
};

class DbArea {
public:
    DbArea();
    ~DbArea();

    // Non-copyable, movable
    DbArea(const DbArea&) = delete;
    DbArea& operator=(const DbArea&) = delete;
    DbArea(DbArea&&) = default;
    DbArea& operator=(DbArea&&) = default;

    void open(const std::string& filename);
    void close();
    bool isOpen() const noexcept { return static_cast<bool>(_fp); }
    bool isDeleted() const;

    // Navigation
    bool gotoRec(int32_t recno);
    bool top();
    bool bottom();
    bool skip(int delta);

    // Record IO
    bool readCurrent();
    bool writeCurrent();
    bool appendBlank();
    bool deleteCurrent();

    // Field access
    const std::vector<FieldDef>& fields() const { return _fields; }
    std::string get(int idx) const;            // 1-based
    bool set(int idx, const std::string& val); // 1-based

    // Info
    int32_t recno() const { return _crn; }
    int32_t recCount() const { return _hdr.num_of_recs; }
    int     fieldCount() const { return static_cast<int>(_fields.size()); }
    int     cpr() const { return _hdr.cpr; }
    std::string name() const { return _db_name; }

private:
    std::fstream _fp;
    std::string _db_name;
    HeaderRec _hdr{};
    std::vector<FieldDef> _fields;
    std::vector<FieldRec> _rawFields;
    std::vector<char> _recbuf;

    // Current record values (1-based index: slot 0 unused)
    std::vector<std::string> _fd;
    // [INDEX PATCH] snapshot of values last read from disk (for oldKey on write)
    std::vector<std::string> _fd_snapshot;

    int32_t _crn{0};
    char _del{NOT_DELETED};

    // [INDEX PATCH] per-area index manager
    std::unique_ptr<xindex::IndexManager> _idx;

    // internals
    void readHeader();
    void readFields();
    bool loadFieldsFromBuffer();
    void storeFieldsToBuffer();
    static std::string rtrim(std::string s);

    // [INDEX PATCH] key helpers
    int  findFieldCI(const std::string& name) const; // returns 1-based idx or 0
    int  firstCharField() const;                     // 1-based idx or 0
    std::vector<uint8_t> encodeKeyFrom(const std::vector<std::string>& vals) const;
    std::vector<uint8_t> currentKey() const { return encodeKeyFrom(_fd); }
    std::vector<uint8_t> snapshotKey() const { return encodeKeyFrom(_fd_snapshot); }
};

class XBaseEngine {
public:
    XBaseEngine();
    DbArea& area(int idx) { if (idx<0 || idx>=MAX_AREA) throw std::out_of_range("area"); return *_areas[idx]; }
    void selectArea(int idx) { if (idx<0 || idx>=MAX_AREA) throw std::out_of_range("area"); _current = idx; }
    int currentArea() const { return _current; }
private:
    std::array<std::unique_ptr<DbArea>, MAX_AREA> _areas;
    int _current{0};
};

// Helpers
std::string dbNameWithExt(std::string s); // ensure .dbf
} // namespace xbase

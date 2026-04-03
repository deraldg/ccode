#include "xbase.hpp"  // Your truth
#include <sqlite3.h>  // Header-only
#include <variant>    // C++17 CellValue
#include <filesystem> // Exports

namespace glorious_bridge {

// Unified types
enum class DataTerm { Cell, Row, Table, Workbook };
using CellValue = std::variant<std::monostate, std::string, double, bool, std::chrono::system_clock::time_point, std::vector<uint8_t>>;  // Blob for memo/OLE

struct TypeMap {
    static char foxToSql(char fox_type) {  // e.g., 'C'?TEXT, 'N'?DECIMAL(p,s), 'M'?BLOB
        switch (fox_type) {
            case 'C': return SQLITE_TEXT; case 'N': return SQLITE_FLOAT; /* etc. */
            default: throw std::invalid_argument("Unsupported Fox type");
        }
    }
    static char sqlToExcel(int sql_type) { /* SQLITE_TEXT?String, etc. */ }
    static char foxToExcel(char fox_type) { /* 'N'?Number, 'D'?Date */ }
};

// Cell: Unified field access
class Cell {
public:
    std::string name;      // Trimmed, CI
    char type;             // Fox/SQL/Excel code
    size_t length = 0;     // Bytes/precision
    size_t decimals = 0;
    CellValue default_val = std::monostate{};

    CellValue get() const;  // From underlying (your getFieldAsString/Number)
    void set(const CellValue& val);  // Coerce + validate (e.g., stod for 'N')
};

// Row: Unified record
class Row {
public:
    bool deleted = false;  // Fox flag
    std::vector<Cell> cells;  // By position
    std::map<std::string, CellValue> named;  // By name (from your resolve_field_index_std)

    Row& operator[](int idx) { return cells[idx]; }  // 0-based
    CellValue& at(const std::string& name);  // CI lookup
};

// Table: Unified table
class Table {
public:
    std::string name;      // DBF basename
    std::vector<Cell> schema;  // Fields/cols
    std::vector<Row> rows;     // Or cursor for large
    size_t row_count = 0;

    Row& current();  // Like your readCurrent()
    bool goto_row(int idx);  // 0-based, skips deleted
    void append(const Row& r);  // Like appendBlank() + set()
    std::vector<Row> select(const std::string& where);  // Simple filter (or full SQL parse)
};

// Workbook: Unified schema/db
class Workbook : public Table {  // Inherit for catalog table
public:
    std::vector<Table> tables;  // DBF files or SQL tables
    std::string path;           // Dir for Fox schema

    Table& table(const std::string& name);  // Open/add
};

// Fox Impl (extends your DbArea)
class FoxTable : public Table {
    xbase::DbArea* area;  // From engine
public:
    FoxTable(xbase::DbArea& a) : area(&a) {
        name = textio::trim(a.name());  // Your helper
        for (auto& f : a.fields()) {  // 0-based ? Cell
            schema.emplace_back(Cell{f.name, f.type, f.length, f.decimals});
        }
        row_count = a.recCount();
    }
    // Overrides: goto_row ? area->gotoRec(idx+1)
    // select: Use your navigation + where eval on getFieldAsString
    // Type map: 'F' as double via stod
};

// Sql Impl
class SqlTable : public Table {
    sqlite3* db;
    sqlite3_stmt* stmt;
public:
    SqlTable(sqlite3* d, const std::string& create_sql) : db(d) {
        sqlite3_exec(db, create_sql.c_str(), nullptr, nullptr, nullptr);  // e.g., "CREATE TABLE t (id INTEGER...)"
    }
    // select: sqlite3_prepare_v2 + step/bind
    // Map schema from PRAGMA table_info
};

// Excel Impl (CSV for simplicity)
class ExcelTable : public Table {
    std::ofstream csv;
public:
    ExcelTable(const std::string& path) : csv(path) { csv << "Sheet1\n"; }  // Or libxl for real XLSX
    void export_row(const Row& r) {
        for (auto& c : r.cells) {
            std::visit([](auto&& v) { csv << std::format("{}", v); }, c.get());  // Coerce
            csv << ",";
        }
        csv << "\n";
    }
};

// Bridge
class Bridge {
    std::unique_ptr<Workbook> fox_wb, sql_wb, excel_wb;
public:
    Bridge(const std::string& fox_dir) {
        fox_wb = std::make_unique<Workbook>(); fox_wb->path = fox_dir;
        // Scan .dbf ? tables
        for (auto& entry : std::filesystem::directory_iterator(fox_dir)) {
            if (entry.path().extension() == ".dbf") {
                xbase::DbArea area; area.open(entry.path());
                fox_wb->tables.emplace_back(FoxTable(area));
            }
        }
        // Init SQL: sqlite3_open(":memory:") or file
        // Init Excel: Optional
    }
    void migrate_fox_to_sql(const std::string& fox_tbl, const std::string& sql_tbl) {
        auto& src = fox_wb->table(fox_tbl);
        // Build CREATE from schema + TypeMap
        std::string create = "CREATE TABLE " + sql_tbl + " (";
        for (auto& c : src.schema) {
            create += std::format("{} {}({}),", c.name, TypeMap::foxTypeToSqlStr(c.type), c.length);
        }
        create.pop_back(); create += ")";
        // Exec on sql_wb
        for (auto& row : src.rows) {  // Or cursor
            std::string insert = "INSERT INTO " + sql_tbl + " VALUES (";
            for (auto& cell : row.cells) {
                insert += std::visit([](auto&& v){ return std::format("?,"); }, cell.get());  // Bind later
            }
            // sqlite3_bind via stmt
        }
    }
    std::vector<Row> query(const std::string& q, DataTerm from) {  // e.g., "SELECT * FROM cust WHERE name='Acme'" or Fox "LOCATE name='Acme'"
        // Parser: If Fox syntax (no SELECT), convert via simple rules (LOCATE?WHERE, etc.)
        // Exec on from model, return unified Rows
    }
    void preview_excel(const std::string& tbl_name) {
        auto& t = fox_wb->table(tbl_name);  // Or SQL
        ExcelTable xls("preview.xlsx");  // CSV impl
        for (auto& r : t.schema) xls.export_row(r);  // Header row
        for (auto& r : t.rows) xls.export_row(r);
    }
};
}  // namespace



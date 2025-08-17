# DotTalk++ API Map
_Generated: 2025-08-15_

This file inventories the key objects (classes/structs/namespaces) and their public surface as used by the CLI. If a method appears here, it’s either present in headers or clearly required by call-sites.

---

## Namespace: `xbase`  — DBF engine

### `struct FieldDef`
- **Members**
  - `std::string name`
  - `char        type`    // 'C','N','D','L', etc.
  - `uint8_t     len`
  - `uint8_t     dec`

### `class DbfFile`
- **Lifecycle**
  - `bool open(const std::string& path)`
  - `void close()`
  - `bool isOpen() const`
- **Header/Schema**
  - `const std::vector<FieldDef>& fields() const`
  - `int  charsPerRecord() const`
- **Navigation**
  - `bool gotoRec(int32_t recno)`
  - `bool top()`
  - `bool bottom()`
  - `bool skip(int delta)`
- **Record I/O**
  - `bool readCurrent()`
  - `bool writeCurrent()`
  - `bool appendBlank()`
  - `bool deleteCurrent()`
- **Record State**
  - `int32_t recordCount() const`
  - `int32_t currentRecNo() const`
  - `char    deletedFlag() const` // ' ' or '*'

### `struct RecordView`
- **Purpose**: typed/pretty access to the current record buffer
- **Key API**
  - `std::string fieldAsString(int fieldIndex, size_t width, bool padForChar) const`

### `class DbArea`
- **Open/Close**
  - `bool use(const std::string& fileStem)`        // opens `<stem>.dbf`
  - `void close()`
  - `bool isOpen() const`
- **Navigation**
  - `bool top()`
  - `bool bottom()`
  - `bool gotoRec(int32_t recno)`
  - `bool skip(int delta)`
- **Introspection**
  - `int32_t recordCount() const`
  - `int32_t currentRecNo() const`
  - `char    deletedFlag() const`
  - `const std::vector<FieldDef>& fields() const`
  - `const RecordView&            view()   const`

---

## Namespace: `xindex` — Index layer

### `struct Key`
- `std::vector<uint8_t> bytes;`

### `struct Range`
- `Key low;`
- `Key high;`
- `bool inclusive_low;`
- `bool inclusive_high;`

### `class IIndexBackend`
- `virtual ~IIndexBackend() = default;`
- **Lifecycle**
  - `virtual bool open(const std::string& path) = 0;`
  - `virtual bool close() = 0;`
- **Staleness**
  - `virtual void setFingerprint(const std::string& fp) = 0;`
  - `virtual bool wasStale() const = 0;`
- **Build/Maintain**
  - `virtual bool rebuild(const std::vector<std::pair<Key,int32_t>>& kv) = 0;`
  - `virtual bool upsert(const Key&, int32_t recno) = 0;`
  - `virtual bool erase (const Key&, int32_t recno) = 0;`
- **Lookup/Scan**
  - `virtual std::optional<int32_t> seek(const Key&) const = 0;`
  - `virtual void scan(const Range&, std::function<void(int32_t)>) const = 0;`
- **Optional cursor**
  - `struct Cursor { virtual bool first(const Range&) = 0; virtual bool next(int32_t& out) = 0; virtual ~Cursor() = default; };`

### `class BPlusTreeBackend : public IIndexBackend`
- On-disk B+Tree implementation (implements full interface).

### `class BptMemBackend : public IIndexBackend`
- In-memory multimap implementation (for tests/simple use).

### `class IndexManager`
- `bool open(const std::string& basePath);`
- `void close();`
- `bool wasStale() const;`
- `bool rebuild(std::function<void(std::function<void(const Key&,int32_t)>)> fill);`
- `std::optional<int32_t> seek(const Key&) const;`
- `void scan(const Range&, std::function<void(int32_t)>) const;`
- `std::unique_ptr<IIndexBackend::Cursor> cursorForKey(const Range&) const;`

---

## Namespace: `cli` — Shell and commands

### `struct Settings` (`include/cli/settings.hpp`)
- `int  page_lines   = 20;`
- `bool show_deleted = false;`

### `class CommandRegistry`
- `using Handler = std::function<void(xbase::DbArea&, std::istringstream&)>;`
- `void add(std::string name, Handler);`
- `bool run(const std::string& name, xbase::DbArea&, std::istringstream&) const;`
- `void help(std::ostream&) const;`

### Free command handlers (definitions in `src/cli/cmd_*.cpp`)
- `void cmd_USE    (xbase::DbArea&, std::istringstream&);`
- `void cmd_LIST   (xbase::DbArea&, std::istringstream&);`
- `void cmd_FIELDS (xbase::DbArea&, std::istringstream&);`
- `void cmd_COUNT  (xbase::DbArea&, std::istringstream&);`
- `void cmd_TOP    (xbase::DbArea&, std::istringstream&);`
- `void cmd_BOTTOM (xbase::DbArea&, std::istringstream&);`
- `void cmd_GOTO   (xbase::DbArea&, std::istringstream&);`
- `void cmd_APPEND (xbase::DbArea&, std::istringstream&);`
- `void cmd_DELETE (xbase::DbArea&, std::istringstream&);`
- `void cmd_RECALL (xbase::DbArea&, std::istringstream&);` // `UNDELETE` alias
- `void cmd_PACK   (xbase::DbArea&, std::istringstream&);`
- `void cmd_DISPLAY(xbase::DbArea&, std::istringstream&);`
- `void cmd_COPY   (xbase::DbArea&, std::istringstream&);`
- `void cmd_EXPORT (xbase::DbArea&, std::istringstream&);`
- `void cmd_IMPORT (xbase::DbArea&, std::istringstream&);`
- `void cmd_FIND   (xbase::DbArea&, std::istringstream&);`
- `void cmd_SEEK   (xbase::DbArea&, std::istringstream&);`
- `void cmd_COLOR  (xbase::DbArea&, std::istringstream&);`
- `void cmd_VERSION(xbase::DbArea&, std::istringstream&);`

> Built-ins handled directly in `shell.cpp`: `HELP`, `AREA`, `SELECT`, `QUIT/EXIT`.

---

## Utilities

### `namespace textio`
- `std::string up(std::string);`
- `void hr(std::ostream& os, int width);`
- `void print_heading(std::ostream& os, std::span<const std::string> cells, std::span<const int> widths);`

### `namespace csv`
- `void write(std::ostream&, const std::vector<std::string>& row);`
- `bool read (std::istream&,  std::vector<std::string>& out_row);`

### `namespace predicates`
- Common predicate helpers for `FIND`/`LOCATE` (string contains/startswith, numeric comparisons, etc.).

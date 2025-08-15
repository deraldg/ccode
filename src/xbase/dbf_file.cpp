#include "xbase.hpp"
// NOTE: removed "utils.hpp" include to avoid current header parser issue
#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>
#include <fstream>

#if DOTTALK_WITH_INDEX
  // xindex scaffold headers (include only in .cpp)
  #include "xindex/index_manager.hpp"
  #include "xindex/index_backend.hpp"
  #include "xindex/key_codec.hpp"
  // Use explicit path so it doesn't depend on include_directories order:
  #include "xindex_scaffold/backends/bpt_backend.hpp"
#endif

namespace xbase {

// --- local, self-contained ends_with_ci (so we don't need utils.hpp right now)
static inline bool ends_with_ci_local(const std::string& s, const std::string& suf) noexcept {
    if (s.size() < suf.size()) return false;
    const size_t off = s.size() - suf.size();
    for (size_t i = 0; i < suf.size(); ++i) {
        const unsigned char a_uc = static_cast<unsigned char>(s[off + i]);
        const unsigned char b_uc = static_cast<unsigned char>(suf[i]);
        const int a = std::tolower(static_cast<int>(a_uc));
        const int b = std::tolower(static_cast<int>(b_uc));
        if (a != b) return false;
    }
    return true;
}

// ---- helpers exposed in header ----
std::string dbNameWithExt(std::string s) {
    while (!s.empty() && s.back()==' ') s.pop_back();
    if (!ends_with_ci_local(s, ".dbf")) s += ".dbf";
    return s;
}

// ---- DbArea: file/open/structure/navigation ----
DbArea::DbArea() {}
DbArea::~DbArea() { close(); }

void DbArea::open(const std::string& filename) {
    close();
    _db_name = filename;
    _fp.open(_db_name, std::ios::in | std::ios::out | std::ios::binary);
    if (!_fp) {
        _fp.clear();
        _fp.open(_db_name, std::ios::out | std::ios::binary);
        _fp.close();
        _fp.open(_db_name, std::ios::in | std::ios::out | std::ios::binary);
    }
    if (!_fp) throw std::runtime_error("Cannot open file: " + _db_name);

    readHeader();
    readFields();
    _recbuf.assign(_hdr.cpr, ' ');
    _fd.assign(_fields.size()+1, std::string{}); // 1-based
    gotoRec(1);

#if DOTTALK_WITH_INDEX
    // Initialize an in-memory index tag for staging
    if (!_idx) {
        _idx = std::make_unique<xindex::IndexManager>();
        auto be = std::make_unique<xindex::BptBackend>(); // in-memory demo backend
        _idx->addTag("TAG1", "<memory>", std::move(be));
        _idx->setActive("TAG1");
    }
#endif
}

void DbArea::close() {
#if DOTTALK_WITH_INDEX
    _idx.reset();
#endif
    if (_fp.is_open()) {
        _fp.flush();
        _fp.close();
    }
    _fields.clear();
    _rawFields.clear();
    _recbuf.clear();
    _fd.clear();
    _fd_snapshot.clear();
    _crn = 0;
}

void DbArea::readHeader() {
    _fp.seekg(0, std::ios::beg);
    _fp.read(reinterpret_cast<char*>(&_hdr), sizeof(HeaderRec));
    if (!_fp) throw std::runtime_error("Failed to read header");
}

void DbArea::readFields() {
    // Number of fields = (data_start - sizeof(HeaderRec) - 1) / sizeof(FieldRec)
    int bytes = _hdr.data_start - static_cast<int>(sizeof(HeaderRec)) - 1;
    int n = bytes / static_cast<int>(sizeof(FieldRec));
    if (n < 0 || n > MAX_FIELDS) throw std::runtime_error("Invalid field count in header");
    _rawFields.resize(n);
    _fields.clear();
    _fields.reserve(n);
    _fp.seekg(sizeof(HeaderRec), std::ios::beg);
    _fp.read(reinterpret_cast<char*>(_rawFields.data()), n * sizeof(FieldRec));
    char term; _fp.read(&term, 1);
    if (!_fp) throw std::runtime_error("Failed to read field descriptors");
    for (int i=0;i<n;++i) {
        FieldDef f{};
        const char* nm = _rawFields[i].field_name;
        std::string name(nm, nm + 11);
        name.erase(std::find(name.begin(), name.end(), '\0'), name.end());
        f.name = name;
        f.type = _rawFields[i].field_type;
        f.length = _rawFields[i].field_length;
        f.decimals = _rawFields[i].decimal_places;
        _fields.push_back(f);
    }
}

bool DbArea::gotoRec(int32_t recno) {
    if (recno < 1 || recno > _hdr.num_of_recs) return false;
    _crn = recno;
    std::streampos pos = _hdr.data_start + static_cast<std::streamoff>((recno-1) * _hdr.cpr);
    _fp.seekg(pos, std::ios::beg);
    return readCurrent();
}

bool DbArea::top()    { return gotoRec(1); }
bool DbArea::bottom() { return gotoRec(_hdr.num_of_recs); }

bool DbArea::skip(int delta) {
    if (_crn == 0) return false;
    int32_t want = _crn + delta;
    if (want < 1 || want > _hdr.num_of_recs) return false;
    return gotoRec(want);
}

bool DbArea::appendBlank() {
    std::vector<char> blank(_hdr.cpr, ' ');
    blank[0] = NOT_DELETED;
    _fp.seekp(0, std::ios::end);
    _fp.write(blank.data(), blank.size());
    if (!_fp) return false;

    _hdr.num_of_recs++;
    _fp.seekp(0, std::ios::beg);
    _fp.write(reinterpret_cast<const char*>(&_hdr), sizeof(HeaderRec));
    _fp.flush();

    bool ok = gotoRec(_hdr.num_of_recs);
#if DOTTALK_WITH_INDEX
    if (ok && _idx) {
        _fd_snapshot = _fd;
        _idx->insert(currentKey(), _hdr.num_of_recs);
    }
#endif
    return ok;
}

bool DbArea::deleteCurrent() {
    if (_crn == 0) return false;
    _del = IS_DELETED;
    bool ok = writeCurrent();
#if DOTTALK_WITH_INDEX
    if (ok && _idx) {
        _idx->erase(snapshotKey(), _crn);
    }
#endif
    return ok;
}

XBaseEngine::XBaseEngine() {
    for (auto& p : _areas) p = std::make_unique<DbArea>();
}

#if DOTTALK_WITH_INDEX
void DbArea::rebuildActiveIndex() {
    if (!_idx) return;
    int32_t saved = recno();
    if (!top()) return;
    do {
        _idx->insert(currentKey(), recno());
    } while (skip(1));
    if (saved > 0) gotoRec(saved);
}
#endif

} // namespace xbase

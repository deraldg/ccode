#include "xbase.hpp"
#include "xindex/index_manager.hpp"  // satisfies forward-declared unique_ptr

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace xbase {

// ---------- helpers ---------------------------------------------------------
static std::string to_upper_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

// prefer path-based API to avoid char8_t/u8string issues
static bool file_exists(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p, ec) && !ec;
}

// ---------- lifecycle --------------------------------------------------------
DbArea::DbArea() = default;
DbArea::~DbArea() { try { close(); } catch(...) {} }

void DbArea::close() {
    if (_idx) {
        _idx->close();
    }

    if (_fp.is_open()) {
        _fp.close();
    }

    // Clear canonical runtime descriptors
    _clear_paths_and_names_();

    // Clear schema/buffers & cursor flags
    _hdr = {};
    _fields.clear();
    _rawFields.clear();
    _recbuf.clear();
    _fd.clear();
    _fd_snapshot.clear();
    _crn = 0;
    _crn64 = 0;
    _rec_count64 = 0;
    _del = NOT_DELETED;
    _memo_kind = MemoKind::NONE;
    _kind = AreaKind::Unknown;

    // Drop per-area managers
    _idx.reset();

    // Legacy mirrors
    _db_name.clear();
    _filename.clear();

    // x64/VFP extras
    _dbf_version_byte = 0x03;
    _autoq_next64 = 0;
    _table_flags = 0;
}

// Legacy helper retained; should update canonical filename
void DbArea::setFilename(std::string path) {
    // Normalize to absolute and compute descriptors
    fs::path p(path);
    std::error_code ec;
    if (!p.is_absolute()) p = fs::absolute(p, ec);

    _compute_paths_and_names_(p.string());

    // Keep legacy mirrors in sync
    _filename  = _dbf_abs_path;
    _db_name   = _logical_name;
}

int DbArea::recordLength() const noexcept {
    return _hdr.cpr;
}

// ---------- runtime capability model ----------------------------------------
bool DbArea::supports(AreaCapability cap) const noexcept
{
    switch (_kind) {
        case AreaKind::V32:
            switch (cap) {
                case AreaCapability::TupleOps:
                    return false;
                default:
                    return true;
            }

        case AreaKind::V64:
            return true;

        case AreaKind::V128:
            return true;

        case AreaKind::Tup:
            switch (cap) {
                case AreaCapability::ReadRows:
                case AreaCapability::TupleOps:
                    return true;
                default:
                    return false;
            }

        case AreaKind::Unknown:
        default:
            return false;
    }
}

// ---------- canonical descriptor computation --------------------------------
void DbArea::_compute_paths_and_names_(const std::string& abs_dbf_path) {
    std::error_code ec;

    // 1) Canonicalize path (prefer weakly_canonical to avoid throws on odd segments)
    fs::path p(abs_dbf_path);
    if (!p.is_absolute()) {
        p = fs::absolute(p, ec); // best effort
    } else {
        fs::path wc = fs::weakly_canonical(p, ec);
        if (!ec && !wc.empty()) p = std::move(wc);
    }

    // 2) Stamp canonical DBF descriptors
    _dbf_abs_path = p.string();
    _dbf_dir      = p.parent_path().string();
    _dbf_ext      = p.has_extension() ? p.extension().string() : std::string{};
    _dbf_basename = p.stem().string();
    _logical_name = to_upper_copy(_dbf_basename);

    // 3) Memo detection (co-located only): prefer .fpt, else .dbt
    const fs::path fpt = p.parent_path() / (_dbf_basename + ".fpt");
    const fs::path dbt = p.parent_path() / (_dbf_basename + ".dbt");

    _memo_abs_path.clear();
    _memo_kind = MemoKind::NONE;

    if (file_exists(fpt)) {
        _memo_abs_path = fpt.string();
        _memo_kind = MemoKind::FPT;
    } else if (file_exists(dbt)) {
        _memo_abs_path = dbt.string();
        _memo_kind = MemoKind::DBT;
    }

    // 4) Keep legacy mirrors synchronized (derived, not authoritative)
    _filename  = _dbf_abs_path;
    _db_name   = _logical_name;
}

void DbArea::_clear_paths_and_names_() noexcept {
    _dbf_abs_path.clear();
    _dbf_dir.clear();
    _dbf_basename.clear();
    _dbf_ext.clear();
    _logical_name.clear();
    _memo_abs_path.clear();
    _memo_kind = MemoKind::NONE;
}

// ---------- index manager access --------------------------------------------
xindex::IndexManager& DbArea::indexManager() {
    if (!_idx) {
        _idx = std::make_unique<xindex::IndexManager>(*this);
    }
    return *_idx;
}

const xindex::IndexManager* DbArea::indexManagerPtr() const noexcept {
    return _idx.get();
}

} // namespace xbase
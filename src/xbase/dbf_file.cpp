// src/xbase/dbf_file.cpp  (ASCII only)

#include "xbase.hpp"
#include "xbase_vfp.hpp"
#include "xbase_64.hpp"
// NOTE: removed "utils.hpp" include to avoid current header parser issue

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#if DOTTALK_WITH_INDEX
  #include "xindex/index_manager.hpp"
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
    while (!s.empty() && s.back() == ' ') s.pop_back();
    if (!ends_with_ci_local(s, ".dbf")) s += ".dbf";
    return s;
}

// ---- DbArea: file/open/structure/navigation ----

void DbArea::open(const std::string& filename)
{
    close();

    namespace fs = std::filesystem;

    fs::path p(filename);
    if (!p.is_absolute()) p = fs::absolute(p);

    // Normalize without full canonicalization.
    const fs::path norm = p.lexically_normal();
    const std::string abs = norm.string();

    if (!fs::exists(norm)) {
        throw std::runtime_error("DbArea: file does not exist: " + abs);
    }
    if (!fs::is_regular_file(norm)) {
        throw std::runtime_error("DbArea: not a regular file: " + abs);
    }

    const auto sz = fs::file_size(norm);
    if (sz < static_cast<std::uintmax_t>(sizeof(HeaderRec))) {
        throw std::runtime_error("DbArea: invalid DBF header (file too small): " + abs);
    }

    // Canonical contract (xbase.hpp): compute + store absolute DBF path + derived names.
    _compute_paths_and_names_(abs);

    // Open using the canonical absolute path (single source of truth).
    // IMPORTANT: Do not auto-create files here. USE/OPEN must never create files.
    _fp.open(_dbf_abs_path, std::ios::in | std::ios::out | std::ios::binary);
    if (!_fp.is_open()) {
        throw std::runtime_error("DbArea: cannot open file (access denied/locked?): " + _dbf_abs_path);
    }

    readHeader();
    readFields();

    _recbuf.assign(static_cast<size_t>(_hdr.cpr), ' ');

    // 1-based field values
    _fd.assign(_fields.size() + 1, std::string{});
    _fd_snapshot.assign(_fields.size() + 1, std::string{});

    // Start at record 1 if non-empty; otherwise remain at BOF.
    if (_hdr.num_of_recs > 0) {
        _crn = 1;
        _crn64 = 1;
        readCurrent();
    } else {
        _crn = 0;
        _crn64 = 0;
        _del = NOT_DELETED;
    }

#if DOTTALK_WITH_INDEX
    // Construct the manager bound to this area. No default ctor exists.
    if (!_idx) {
        _idx = std::make_unique<xindex::IndexManager>(*this);
    }
#endif
}

void DbArea::readHeader()
{
    if (!_fp.is_open()) {
        throw std::runtime_error("DbArea: file not open in readHeader");
    }

    _fp.clear();
    _fp.seekg(0, std::ios::beg);

    const std::uint8_t ver = vfp_loader::peekVersion(_fp);

    if (ver == DBF_VERSION_64) {
        x64_loader::readHeader(*this, _fp);
        return;
    }

    // Use the VFP/legacy loader for classic, Fox26, and VFP DBFs.
    vfp_loader::readHeader(*this, _fp);
}

void DbArea::readFields()
{
    if (!_fp.is_open()) {
        throw std::runtime_error("DbArea: file not open in readFields");
    }

    _fp.clear();
    _fp.seekg(0, std::ios::beg);

    const std::uint8_t ver = vfp_loader::peekVersion(_fp);
    std::vector<VfpFieldExtras> extras;

    if (ver == DBF_VERSION_64) {
        x64_loader::readFields(*this, _fp, extras);
        return;
    }

    // Current vfp_loader::readFields() contract:
    // caller positions the stream just after the 32-byte header.
    // This applies to classic (0x03/0x83/0xF5) and VFP (0x30/0x31/0x32),
    // since both use 32-byte headers in the current implementation.
    _fp.seekg(sizeof(HeaderRec), std::ios::beg);
    vfp_loader::readFields(*this, _fp, extras);

    // extras currently remain loader-local by design.
    // If/when VFP nullable/binary/autoinc metadata becomes first-class runtime
    // state, store them on DbArea here rather than re-parsing elsewhere.
}

bool DbArea::gotoRec(int32_t recno) {
    if (recno < 1 || recno > _hdr.num_of_recs) return false;

    _crn = recno;
    _crn64 = static_cast<std::uint64_t>(recno);

    const std::streampos pos =
        static_cast<std::streampos>(_hdr.data_start) +
        static_cast<std::streamoff>((recno - 1) * _hdr.cpr);

    _fp.seekg(pos, std::ios::beg);
    return readCurrent();
}

bool DbArea::top()    { return gotoRec(1); }
bool DbArea::bottom() { return gotoRec(_hdr.num_of_recs); }

bool DbArea::skip(int delta) {
    if (_crn == 0) return false;
    const int32_t want = _crn + delta;
    if (want < 1 || want > _hdr.num_of_recs) return false;
    return gotoRec(want);
}

bool DbArea::appendBlank() {
    // Empty freshly-opened tables can leave the stream in fail/eof state because
    // open() positions to BOF when num_of_recs == 0.
    _fp.clear();

    std::vector<char> blank(static_cast<size_t>(_hdr.cpr), ' ');
    blank[0] = NOT_DELETED;

    // For an empty DBF created with a trailing 0x1A EOF marker, append should
    // overwrite that marker with the new record and then write a new EOF marker.
    _fp.seekg(0, std::ios::end);
    const std::streamoff fileSize = _fp.tellg();

    std::streamoff recPos = fileSize;

    if (fileSize > 0) {
        char last = 0;
        _fp.seekg(fileSize - 1, std::ios::beg);
        _fp.read(&last, 1);
        _fp.clear();

        if (static_cast<unsigned char>(last) == 0x1A) {
            recPos = fileSize - 1;
        }
    }

    _fp.seekp(recPos, std::ios::beg);
    _fp.write(blank.data(), static_cast<std::streamsize>(blank.size()));

    const char eof = static_cast<char>(0x1A);
    _fp.write(&eof, 1);

    if (!_fp) return false;

    _hdr.num_of_recs++;
    _rec_count64 = static_cast<std::uint64_t>(_hdr.num_of_recs);

    // Do not rewrite the entire 32-byte header through HeaderRec for VFP/x64.
    // That would clobber dialect-specific tail bytes.
    // Patch the common 32-bit record count at offset 4.
    _fp.clear();
    _fp.seekp(4, std::ios::beg);
    _fp.write(reinterpret_cast<const char*>(&_hdr.num_of_recs), sizeof(_hdr.num_of_recs));

    // x64 dialect also keeps the authoritative record count in the extension block
    // immediately after the 32-byte VFP-style header.
    if (_dbf_version_byte == DBF_VERSION_64) {
        const std::uint64_t rc64 = _rec_count64;
        _fp.seekp(static_cast<std::streamoff>(sizeof(VfpHeader)) +
                      static_cast<std::streamoff>(offsetof(LargeHeaderExtension, record_count)),
                  std::ios::beg);
        _fp.write(reinterpret_cast<const char*>(&rc64), sizeof(rc64));
    }

    _fp.flush();

    if (!_fp) return false;

    _fp.clear();

    // Defensive reset: do not let the previous current record leak into the
    // newly appended blank row if the decode path is imperfect.
    _recbuf.assign(static_cast<size_t>(_hdr.cpr), ' ');
    _fd.assign(_fields.size() + 1, std::string{});
    _fd_snapshot.assign(_fields.size() + 1, std::string{});
    _del = NOT_DELETED;

    const bool ok = gotoRec(_hdr.num_of_recs);

#if DOTTALK_WITH_INDEX
    if (ok && _idx && _idx->has_active()) {
        _fd_snapshot = _fd;
        _idx->on_append(_hdr.num_of_recs);
    }
#endif

    return ok;
}

bool DbArea::deleteCurrent() {
    if (_crn == 0) return false;
    _del = IS_DELETED;

    const bool ok = writeCurrent();

#if DOTTALK_WITH_INDEX
    if (ok && _idx && _idx->has_active()) {
        _idx->on_delete(_crn);
    }
#endif

    return ok;
}

XBaseEngine::XBaseEngine() {
    for (auto& p : _areas) p = std::make_unique<DbArea>();
}

} // namespace xbase
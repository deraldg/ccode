#include "xbase.hpp"
#include "textio.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#if DOTTALK_WITH_INDEX
  #include "xindex/index_manager.hpp"
  #include "xindex/key_codec.hpp"   // xindex::codec::encodeChar(...)
#endif

namespace xbase {

bool DbArea::readCurrent() {
    if (_crn == 0) return false;
    std::streampos pos = _hdr.data_start + static_cast<std::streamoff>((_crn-1) * _hdr.cpr);
    _fp.seekg(pos, std::ios::beg);
    _fp.read(_recbuf.data(), _recbuf.size());
    if (!_fp) return false;
    _del = _recbuf[0];
    return loadFieldsFromBuffer();
}

bool DbArea::isDeleted() const {
    if (!_recbuf.empty())           // delete flag is first byte of DBF record
        return _recbuf[0] == IS_DELETED;
    return _del == IS_DELETED;      // fallback if buffer isn’t loaded
}

bool DbArea::writeCurrent() {
    if (_crn == 0) return false;
    storeFieldsToBuffer();
    std::streampos pos = _hdr.data_start + static_cast<std::streamoff>((_crn-1) * _hdr.cpr);
    _fp.seekp(pos, std::ios::beg);
    _fp.write(_recbuf.data(), _recbuf.size());
    _fp.flush();
    bool ok = static_cast<bool>(_fp);

#if DOTTALK_WITH_INDEX
    if (ok && _idx && _idx->has_active()) {
        // If field values changed in a way that affects the key, notify manager.
        // We used to compute old/new keys and call _idx->update(old,new,recno),
        // but the current API exposes on_replace(recno) to recompute for the active tag.
        _idx->on_replace(_crn);
        _fd_snapshot = _fd;
    }
#endif

    return ok;
}

std::string DbArea::get(int idx) const {
    if (idx < 1 || idx > static_cast<int>(_fields.size())) return {};
    return _fd[idx];
}

bool DbArea::set(int idx, const std::string& val) {
    if (idx < 1 || idx > static_cast<int>(_fields.size())) return false;
    _fd[idx] = val;
    return true;
}

bool DbArea::loadFieldsFromBuffer() {
    _fd.assign(_fields.size()+1, std::string{});
    size_t off = 1; // first byte is deleted flag
    for (size_t i=0;i<_fields.size();++i) {
        const auto& f = _fields[i];
        if (off + f.length > _recbuf.size()) return false;
        std::string v(_recbuf.data()+off, _recbuf.data()+off+f.length);
        _fd[i+1] = rtrim(v);
        off += f.length;
    }
#if DOTTALK_WITH_INDEX
    _fd_snapshot = _fd; // snapshot post-read
#endif
    return true;
}

void DbArea::storeFieldsToBuffer() {
    std::fill(_recbuf.begin(), _recbuf.end(), ' ');
    _recbuf[0] = _del;
    size_t off = 1;
    for (size_t i=0;i<_fields.size();++i) {
        const auto& f = _fields[i];
        std::string v = _fd[i+1];
        if (v.size() > f.length) v.resize(f.length);
        std::memcpy(_recbuf.data()+off, v.data(), v.size());
        off += f.length;
    }
}

std::string DbArea::rtrim(std::string s) {
    while (!s.empty() && s.back()==' ') s.pop_back();
    return s;
}

// ---- [INDEX helpers] ----

int DbArea::findFieldCI(const std::string& name) const {
    for (size_t i=0;i<_fields.size();++i) {
        if (textio::ieq(_fields[i].name, name)) return static_cast<int>(i+1);
    }
    return 0;
}

int DbArea::firstCharField() const {
    for (size_t i=0;i<_fields.size();++i) {
        if (_fields[i].type == 'C') return static_cast<int>(i+1);
    }
    return 0;
}

std::vector<uint8_t> DbArea::encodeKeyFrom(const std::vector<std::string>& vals) const {
#if DOTTALK_WITH_INDEX
    // Simple single-character-field key using lexicographic encoding.
    const int idx = firstCharField();
    if (idx <= 0) return {};
    const size_t n = static_cast<size_t>(idx - 1);
    if (n >= vals.size()) return {};

    // Width: use the field length for the first char field.
    const auto& f = _fields[idx - 1];
    const std::size_t width = static_cast<std::size_t>(f.length);

    // Uppercase compare to mimic case-insensitive order (or flip to false if you prefer).
    const bool upper = true;

    return xindex::codec::encodeChar(vals[n], width, upper);
#else
    (void)vals;
    return {};
#endif
}

} // namespace xbase

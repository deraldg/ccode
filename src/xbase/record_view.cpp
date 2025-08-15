#include "xbase.hpp"
#include "textio.hpp"
#include <algorithm>
#include <cstring>

#if DOTTALK_WITH_INDEX
  #include "xindex/key_codec.hpp"
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

bool DbArea::writeCurrent() {
    if (_crn == 0) return false;
    storeFieldsToBuffer();
    std::streampos pos = _hdr.data_start + static_cast<std::streamoff>((_crn-1) * _hdr.cpr);
    _fp.seekp(pos, std::ios::beg);
    _fp.write(_recbuf.data(), _recbuf.size());
    _fp.flush();
    bool ok = static_cast<bool>(_fp);
#if DOTTALK_WITH_INDEX
    if (ok && _idx) {
        auto oldK = snapshotKey();
        auto newK = currentKey();
        if (oldK != newK) {
            _idx->update(oldK, newK, _crn);
        }
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

// ---- [INDEX PATCH] helpers ----

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
    // Stage 1: single CHAR field key (case-insensitive, trimmed)
    int idx = firstCharField();
    if (idx <= 0) return {};
    size_t n = static_cast<size_t>(idx - 1);
    if (n >= vals.size()) return {};
    xindex::KeyOptions opt;
    opt.case_insensitive = true;
    opt.trim_right_spaces = true;
    return xindex::KeyCodec::encodeChar(vals[n], opt);
#else
    (void)vals;
    return {};
#endif
}

} // namespace xbase

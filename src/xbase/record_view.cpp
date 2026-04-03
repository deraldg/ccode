#include "xbase.hpp"
#include "textio.hpp"
#include "xbase_64.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include <cstdint>

#if DOTTALK_WITH_INDEX
  #include "xindex/index_manager.hpp"
  #include "xindex/key_codec.hpp"
#endif

namespace xbase {

namespace {

static inline bool is_memo_type(const FieldDef& f) noexcept
{
    return (f.type == 'M' || f.type == 'm');
}

static inline bool is_x64_memo_field_fast(bool is_x64, const FieldDef& f) noexcept
{
    return is_x64 && is_memo_type(f) && f.length == X64_MEMO_FIELD_LEN;
}

static inline std::uint64_t read_u64_le(const char* p) noexcept
{
    std::uint64_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

static inline void write_u64_le(char* p, std::uint64_t v) noexcept
{
    std::memcpy(p, &v, sizeof(v));
}

} // namespace

bool DbArea::readCurrent()
{
    if (_crn == 0) return false;

    std::streampos pos =
        _hdr.data_start + static_cast<std::streamoff>((_crn - 1) * _hdr.cpr);

    _fp.seekg(pos, std::ios::beg);
    _fp.read(_recbuf.data(), _recbuf.size());
    if (!_fp) return false;

    _del = _recbuf[0];
    return loadFieldsFromBuffer();
}

bool DbArea::isDeleted() const
{
    if (!_recbuf.empty())
        return _recbuf[0] == IS_DELETED;

    return _del == IS_DELETED;
}

bool DbArea::writeCurrent()
{
    if (_crn == 0) return false;

    storeFieldsToBuffer();

    std::streampos pos =
        _hdr.data_start + static_cast<std::streamoff>((_crn - 1) * _hdr.cpr);

    _fp.seekp(pos, std::ios::beg);
    _fp.write(_recbuf.data(), _recbuf.size());
    _fp.flush();

    bool ok = static_cast<bool>(_fp);

#if DOTTALK_WITH_INDEX
    if (ok && _idx && _idx->has_active()) {
        _idx->on_replace(_crn);
        _fd_snapshot = _fd;
    }
#endif

    return ok;
}

std::string DbArea::get(int idx) const
{
    if (idx < 1 || idx > static_cast<int>(_fields.size())) return {};
    return _fd[idx];
}

bool DbArea::set(int idx, const std::string& val)
{
    if (idx < 1 || idx > static_cast<int>(_fields.size())) return false;
    _fd[idx] = val;
    return true;
}

bool DbArea::loadFieldsFromBuffer()
{
    _fd.assign(_fields.size() + 1, std::string{});

    const bool is_x64 = (versionByte() == DBF_VERSION_64);

    size_t off = 1;

    for (size_t i = 0; i < _fields.size(); ++i) {
        const auto& f = _fields[i];

        if (off + f.length > _recbuf.size())
            return false;

        if (is_x64_memo_field_fast(is_x64, f)) {
            const std::uint64_t object_id = read_u64_le(_recbuf.data() + off);

            if (object_id == 0)
                _fd[i + 1].clear();
            else
                _fd[i + 1] = std::to_string(object_id);

        } else {
            // Legacy memo fields are plain fixed-width text, same as other char-ish storage.
            std::string v(_recbuf.data() + off, _recbuf.data() + off + f.length);
            _fd[i + 1] = rtrim(std::move(v));
        }

        off += f.length;
    }

#if DOTTALK_WITH_INDEX
    _fd_snapshot = _fd;
#endif

    return true;
}

void DbArea::storeFieldsToBuffer()
{
    const bool is_x64 = (versionByte() == DBF_VERSION_64);

    std::fill(_recbuf.begin(), _recbuf.end(), ' ');
    _recbuf[0] = _del;

    size_t off = 1;

    for (size_t i = 0; i < _fields.size(); ++i) {
        const auto& f = _fields[i];
        const std::string& src = _fd[i + 1];

        if (is_x64_memo_field_fast(is_x64, f)) {
            std::uint64_t object_id = 0;

            if (!src.empty()) {
                try {
                    object_id = std::stoull(src);
                }
                catch (...) {
                    object_id = 0;
                }
            }

            write_u64_le(_recbuf.data() + off, object_id);

        } else {
            if (!src.empty()) {
                if (src.size() <= f.length)
                    std::memcpy(_recbuf.data() + off, src.data(), src.size());
                else
                    std::memcpy(_recbuf.data() + off, src.data(), f.length);
            }
        }

        off += f.length;
    }
}

std::string DbArea::rtrim(std::string s)
{
    while (!s.empty() && s.back() == ' ')
        s.pop_back();
    return s;
}

// ---- [INDEX helpers] ----

int DbArea::findFieldCI(const std::string& name) const
{
    for (size_t i = 0; i < _fields.size(); ++i) {
        if (textio::ieq(_fields[i].name, name))
            return static_cast<int>(i + 1);
    }
    return 0;
}

int DbArea::firstCharField() const
{
    for (size_t i = 0; i < _fields.size(); ++i) {
        if (_fields[i].type == 'C')
            return static_cast<int>(i + 1);
    }
    return 0;
}

std::vector<uint8_t> DbArea::encodeKeyFrom(const std::vector<std::string>& vals) const
{
#if DOTTALK_WITH_INDEX
    const int idx = firstCharField();
    if (idx <= 0) return {};

    const size_t n = static_cast<size_t>(idx - 1);
    if (n >= vals.size()) return {};

    const auto& f = _fields[idx - 1];
    const std::size_t width = static_cast<std::size_t>(f.length);
    const bool upper = true;

    return xindex::codec::encodeChar(vals[n], width, upper);
#else
    (void)vals;
    return {};
#endif
}

} // namespace xbase
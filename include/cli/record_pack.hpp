#pragma once
// Plan A: Field-level packers into a record data buffer (no flag byte).
// Caller is responsible for copying into the full record with deleted flag prefix.
#include "field_meta.hpp"
#include "field_codecs.hpp"
#include "hex_dump.hpp"
#include <functional>
#include <vector>
#include <string>
#include <iostream>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <algorithm>
#include <cctype>

namespace cli_planA {

struct PackResult {
    bool ok{true};
    std::string fieldName;
    std::string error; // empty if ok
};

// memoWriter: optional callback returning a block id for text payloads
using MemoWriter = std::function<uint32_t(const std::string& text)>;

namespace detail {

inline std::string trim_copy(const std::string& s)
{
    auto issp = [](unsigned char c) { return std::isspace(c) != 0; };

    std::size_t b = 0;
    while (b < s.size() && issp(static_cast<unsigned char>(s[b]))) ++b;

    std::size_t e = s.size();
    while (e > b && issp(static_cast<unsigned char>(s[e - 1]))) --e;

    return s.substr(b, e - b);
}

inline bool parse_i32_strict(const std::string& s, std::int32_t& out)
{
    const std::string t = trim_copy(s);
    if (t.empty()) return false;

    std::int32_t v = 0;
    const char* first = t.data();
    const char* last  = t.data() + t.size();
    auto r = std::from_chars(first, last, v);
    if (r.ec != std::errc() || r.ptr != last) return false;

    out = v;
    return true;
}

inline bool parse_double_strict(const std::string& s, double& out)
{
    const std::string t = trim_copy(s);
    if (t.empty()) return false;

    char* end = nullptr;
    const double v = std::strtod(t.c_str(), &end);
    if (end == t.c_str()) return false;
    if (*end != '\0') return false;
    if (!std::isfinite(v)) return false;

    out = v;
    return true;
}

inline bool parse_currency_10000(const std::string& s, std::int64_t& out)
{
    double v = 0.0;
    if (!parse_double_strict(s, v)) return false;

    const double scaled = v * 10000.0;
    if (!std::isfinite(scaled)) return false;

    const double lo = static_cast<double>(std::numeric_limits<std::int64_t>::min());
    const double hi = static_cast<double>(std::numeric_limits<std::int64_t>::max());
    if (scaled < lo || scaled > hi) return false;

    out = static_cast<std::int64_t>(std::llround(scaled));
    return true;
}

inline std::string pack_le_i32(std::int32_t v, int len)
{
    std::string out(std::max(len, 4), '\0');
    std::uint32_t u = static_cast<std::uint32_t>(v);

    out[0] = static_cast<char>((u >> 0)  & 0xFF);
    out[1] = static_cast<char>((u >> 8)  & 0xFF);
    out[2] = static_cast<char>((u >> 16) & 0xFF);
    out[3] = static_cast<char>((u >> 24) & 0xFF);

    if (len > 4) {
        std::fill(out.begin() + 4, out.begin() + len, '\0');
    }
    if (static_cast<int>(out.size()) > len) out.resize(len);
    return out;
}

inline std::string pack_le_i64(std::int64_t v, int len)
{
    std::string out(std::max(len, 8), '\0');
    std::uint64_t u = static_cast<std::uint64_t>(v);

    for (int i = 0; i < 8; ++i) {
        out[i] = static_cast<char>((u >> (i * 8)) & 0xFF);
    }

    if (len > 8) {
        std::fill(out.begin() + 8, out.begin() + len, '\0');
    }
    if (static_cast<int>(out.size()) > len) out.resize(len);
    return out;
}

inline std::string pack_le_f64(double v, int len)
{
    static_assert(sizeof(double) == 8, "double must be 8 bytes");

    std::string out(std::max(len, 8), '\0');
    std::uint64_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));

    for (int i = 0; i < 8; ++i) {
        out[i] = static_cast<char>((bits >> (i * 8)) & 0xFF);
    }

    if (len > 8) {
        std::fill(out.begin() + 8, out.begin() + len, '\0');
    }
    if (static_cast<int>(out.size()) > len) out.resize(len);
    return out;
}

} // namespace detail

// Packs a single field's user-facing string value into the record data bytes at meta.offset.
inline PackResult pack_field(std::vector<char>& recordData,
                             const FieldMeta& meta,
                             const std::string& userValue,
                             MemoWriter memoWriter = nullptr,
                             bool asciiMemoPtr = false)
{
    PackResult res; res.fieldName = meta.name;
    auto boundsOK = [&](int need) -> bool {
        return meta.offset + (std::size_t)need <= recordData.size();
    };

    switch (meta.type) {
        case 'C': case 'c': {
            std::string bytes = pack_char(userValue, meta.length);
            if (!boundsOK(meta.length)) { res.ok=false; res.error="bounds"; return res; }
            std::copy(bytes.begin(), bytes.end(), recordData.begin() + meta.offset);
            return res;
        }
        case 'N': case 'n': case 'F': case 'f': {
            bool ok = true;
            std::string bytes = pack_numeric_from_string(userValue, meta.length, meta.decimal, ok);
            if (!ok) { res.ok=false; res.error="numeric overflow or invalid"; return res; }
            if (!boundsOK(meta.length)) { res.ok=false; res.error="bounds"; return res; }
            std::copy(bytes.begin(), bytes.end(), recordData.begin() + meta.offset);
            return res;
        }
        case 'D': case 'd': {
            bool ok=true;
            std::string ymd = pack_date(userValue, ok);
            if (!ok) { res.ok=false; res.error="invalid date"; return res; }
            if (meta.length < 8) { res.ok=false; res.error="date field too short"; return res; }
            if (!boundsOK(8)) { res.ok=false; res.error="bounds"; return res; }
            std::copy(ymd.begin(), ymd.end(), recordData.begin() + meta.offset);
            // Pad any remaining length with spaces
            for (int i = 8; i < meta.length; ++i) recordData[meta.offset + i] = ' ';
            return res;
        }
        case 'L': case 'l': {
            if (!boundsOK(std::max(1, meta.length))) { res.ok=false; res.error="bounds"; return res; }
            char c = pack_logical_from_string(userValue);
            recordData[meta.offset] = c;
            // pad rest of length with spaces if length>1
            for (int i = 1; i < meta.length; ++i) recordData[meta.offset + i] = ' ';
            return res;
        }
        case 'I': case 'i': {
            std::int32_t v = 0;
            if (!detail::parse_i32_strict(userValue, v)) {
                res.ok = false;
                res.error = "invalid int32";
                return res;
            }
            if (meta.length < 4) {
                res.ok = false;
                res.error = "int32 field too short";
                return res;
            }
            if (!boundsOK(meta.length)) { res.ok=false; res.error="bounds"; return res; }

            std::string bytes = detail::pack_le_i32(v, meta.length);
            std::copy(bytes.begin(), bytes.end(), recordData.begin() + meta.offset);
            return res;
        }
        case 'B': case 'b': {
            double v = 0.0;
            if (!detail::parse_double_strict(userValue, v)) {
                res.ok = false;
                res.error = "invalid double";
                return res;
            }
            if (meta.length < 8) {
                res.ok = false;
                res.error = "double field too short";
                return res;
            }
            if (!boundsOK(meta.length)) { res.ok=false; res.error="bounds"; return res; }

            std::string bytes = detail::pack_le_f64(v, meta.length);
            std::copy(bytes.begin(), bytes.end(), recordData.begin() + meta.offset);
            return res;
        }
        case 'Y': case 'y': {
            std::int64_t scaled = 0;
            if (!detail::parse_currency_10000(userValue, scaled)) {
                res.ok = false;
                res.error = "invalid currency";
                return res;
            }
            if (meta.length < 8) {
                res.ok = false;
                res.error = "currency field too short";
                return res;
            }
            if (!boundsOK(meta.length)) { res.ok=false; res.error="bounds"; return res; }

            std::string bytes = detail::pack_le_i64(scaled, meta.length);
            std::copy(bytes.begin(), bytes.end(), recordData.begin() + meta.offset);
            return res;
        }
        case 'M': case 'm': case 'G': case 'g': {
            if (!boundsOK(meta.length)) { res.ok=false; res.error="bounds"; return res; }
            uint32_t block_id = 0;
            if (memoWriter) {
                block_id = memoWriter(userValue);
            } // else keep 0 pointer
            std::string ptr = asciiMemoPtr ? pack_memo_ptr_ascii(block_id, meta.length)
                                           : pack_memo_ptr_le32(block_id, meta.length);
            std::copy(ptr.begin(), ptr.end(), recordData.begin() + meta.offset);
            return res;
        }
        default: {
            // Unknown type: write spaces
            if (!boundsOK(meta.length)) { res.ok=false; res.error="bounds"; return res; }
            std::fill(recordData.begin() + meta.offset, recordData.begin() + meta.offset + meta.length, ' ');
            res.ok=false; res.error="unknown type";
            return res;
        }
    }
}

// Build a full on-disk record: deletedFlag + recordData
inline std::vector<char> build_record(char deletedFlag, const std::vector<char>& recordData) {
    std::vector<char> rec;
    rec.reserve(recordData.size() + 1);
    rec.push_back(deletedFlag);
    rec.insert(rec.end(), recordData.begin(), recordData.end());
    return rec;
}

// Simple tracer to std::cerr
inline void trace_pack(const std::string& table,
                       int recno,
                       const std::vector<FieldMeta>& metas,
                       const std::vector<char>& recordData,
                       const std::vector<std::string>& changedFields)
{
    std::cerr << "[trace] table=" << table << " recno=" << recno
              << " changed=" << changedFields.size()
              << " bytes=" << recordData.size() << "\n";
    std::cerr << "[trace] hex:\n" << hex_dump(recordData) << "\n";
    (void)metas;
}

} // namespace cli_planA

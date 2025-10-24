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

namespace cli_planA {

struct PackResult {
    bool ok{true};
    std::string fieldName;
    std::string error; // empty if ok
};

// memoWriter: optional callback returning a block id for text payloads
using MemoWriter = std::function<uint32_t(const std::string& text)>;

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
            if (!ok) { res.ok=false; res.error="numeric overflow or invalid"; }
            if (!boundsOK(meta.length)) { res.ok=false; res.error="bounds"; return res; }
            std::copy(bytes.begin(), bytes.end(), recordData.begin() + meta.offset);
            return res;
        }
        case 'D': case 'd': {
            bool ok=true;
            std::string ymd = pack_date(userValue, ok);
            if (!ok) { res.ok=false; res.error="invalid date"; }
            if (meta.length < 8) { res.ok=false; res.error="date field too short"; return res; }
            if (!boundsOK(8)) { res.ok=false; res.error="bounds"; return res; }
            std::copy(ymd.begin(), ymd.end(), recordData.begin() + meta.offset);
            // Pad any remaining length with spaces
            for (int i = 8; i < meta.length; ++i) recordData[meta.offset + i] = ' ';
            return res;
        }
        case 'L': case 'l': {
            if (!boundsOK(1)) { res.ok=false; res.error="bounds"; return res; }
            char c = pack_logical_from_string(userValue);
            recordData[meta.offset] = c;
            // pad rest of length with spaces if length>1
            for (int i = 1; i < meta.length; ++i) recordData[meta.offset + i] = ' ';
            return res;
        }
        case 'M': case 'm': case 'B': case 'b': case 'G': case 'g': {
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

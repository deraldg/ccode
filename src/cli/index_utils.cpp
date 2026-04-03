#include "cli/index_utils.hpp"
#include "textio.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace dottalk {

std::unordered_map<std::string, MemoryIndex> g_memoryIndexes;

// ---------------- string helpers ----------------

std::string canon(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return r;
}

std::string lower_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string upper_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

// ---------------- comparison ----------------

bool entry_less(const Entry& a, const Entry& b) {
    if (a.key == b.key) return a.recno < b.recno;
    return a.key < b.key;
}

// ---------------- sorting ----------------

void std_sort_algo(std::vector<Entry>& v) {
    std::sort(v.begin(), v.end(), entry_less);
}

void bubble_sort_algo(std::vector<Entry>& v) {
    for (std::size_t i = 0; i < v.size(); ++i) {
        for (std::size_t j = 0; j + 1 < v.size(); ++j) {
            if (entry_less(v[j + 1], v[j])) {
                std::swap(v[j], v[j + 1]);
            }
        }
    }
}

// ---------------- field resolution ----------------

int resolve_field_index(const xbase::DbArea& A, const std::string& tok)
{
    int idx = 0;
    if (is_hashnum(tok, idx)) return idx;

    const auto& Fs = A.fields();
    const std::string t = canon(tok);

    for (std::size_t i = 0; i < Fs.size(); ++i) {
        if (canon(Fs[i].name) == t) {
            return static_cast<int>(i + 1);
        }
    }

    return -1;
}

// ---------------- hash numeric ----------------

bool is_hashnum(const std::string& tok, int& out)
{
    if (tok.size() < 2 || tok[0] != '#') return false;

    try {
        out = std::stoi(tok.substr(1));
        return true;
    } catch (...) {
        return false;
    }
}

// ---------------- memory index lookup ----------------

const MemoryIndex* get_memory_index(const std::string& tag)
{
    auto it = g_memoryIndexes.find(canon(tag));
    if (it == g_memoryIndexes.end()) return nullptr;
    return &it->second;
}

// ---------------- internal entry helpers ----------------

namespace {

std::string normalize_key_for_format(std::string key,
                                     const xbase::FieldDef& fd,
                                     InxFmt fmt)
{
    // 2INX rules: uppercase + fixed-length key for character fields.
    // 1INX stores variable-length keys and keeps the raw value.
    if (fmt == InxFmt::V2_2INX && fd.type == 'C') {
        key = upper_copy(std::move(key));

        if (key.size() < static_cast<std::size_t>(fd.length)) {
            key.append(static_cast<std::size_t>(fd.length) - key.size(), ' ');
        } else if (key.size() > static_cast<std::size_t>(fd.length)) {
            key.resize(static_cast<std::size_t>(fd.length));
        }
    }

    return key;
}

std::vector<Entry> collect_entries_unsorted(xbase::DbArea& A,
                                            int fieldIndex,
                                            const xbase::FieldDef& fd,
                                            InxFmt fmt)
{
    std::vector<Entry> ents;

    const int32_t recCount = A.recCount();
    if (recCount <= 0) return ents;

    ents.reserve(static_cast<std::size_t>(recCount));

    for (int32_t recno = 1; recno <= recCount; ++recno) {
        if (!A.gotoRec(recno)) continue;
        if (!A.readCurrent()) continue;
        if (A.isDeleted()) continue;

        std::string key = A.get(fieldIndex);
        key = normalize_key_for_format(std::move(key), fd, fmt);

        ents.push_back(Entry{ std::move(key), static_cast<uint32_t>(recno) });
    }

    return ents;
}

} // anonymous namespace

// ---------------- entry builder ----------------

std::vector<Entry> create_sorted_entries(
    xbase::DbArea& A,
    int fieldIndex,
    InxFmt fmt,
    uint16_t& keylen,
    char& ftype,
    const std::function<void(std::vector<Entry>&)>& sorter)
{
    std::vector<Entry> ents;

    const auto& Fs = A.fields();
    const std::size_t idx0 = static_cast<std::size_t>(fieldIndex - 1);
    if (fieldIndex < 1 || idx0 >= Fs.size()) {
        keylen = 0;
        ftype = 0;
        return ents;
    }

    const auto& fd = Fs[idx0];

    ftype  = fd.type;
    keylen = static_cast<uint16_t>(fd.length);

    ents = collect_entries_unsorted(A, fieldIndex, fd, fmt);

    if (sorter) {
        sorter(ents);
    } else {
        std_sort_algo(ents);
    }

    return ents;
}

// ---------------- searching ----------------

std::vector<uint32_t> binary_search_algo(
    const std::vector<Entry>& ents,
    const std::string& key)
{
    std::vector<uint32_t> out;
    if (ents.empty()) return out;

    const Entry probe{ key, 0u };

    auto lo = std::lower_bound(ents.begin(), ents.end(), probe, entry_less);
    auto hi = std::upper_bound(ents.begin(), ents.end(), probe, entry_less);

    out.reserve(static_cast<std::size_t>(std::distance(lo, hi)));
    for (auto it = lo; it != hi; ++it) {
        out.push_back(it->recno);
    }
    return out;
}

std::vector<uint32_t> search_memory_index(
    const std::string& tag,
    const std::string& key,
    const std::function<std::vector<uint32_t>(const std::vector<Entry>&, const std::string&)>& algo)
{
    const MemoryIndex* mi = get_memory_index(tag);
    if (!mi) return {};

    if (algo) {
        return algo(mi->entries, key);
    }
    return binary_search_algo(mi->entries, key);
}

} // namespace dottalk
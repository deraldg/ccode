#pragma once

#include "xbase.hpp"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace dottalk {

enum class InxFmt {
    V1_1INX,
    V2_2INX
};

struct Entry {
    std::string key;
    uint32_t recno;
};

struct MemoryIndex {
    std::string expr;
    std::vector<Entry> entries;
    uint16_t keylen{};
    char ftype{};
    int32_t recCount{};
};

extern std::unordered_map<std::string, MemoryIndex> g_memoryIndexes;

// string helpers
std::string canon(const std::string& s);
std::string lower_copy(std::string s);
std::string upper_copy(std::string s);

// field utilities
int resolve_field_index(const xbase::DbArea& A, const std::string& tok);

// entry comparison
bool entry_less(const Entry& a, const Entry& b);

// sorting algorithms
void std_sort_algo(std::vector<Entry>& v);
void bubble_sort_algo(std::vector<Entry>& v);

// index creation
std::vector<Entry> create_sorted_entries(
    xbase::DbArea& A,
    int fieldIndex,
    InxFmt fmt,
    uint16_t& keylen,
    char& ftype,
    const std::function<void(std::vector<Entry>&)>& sorter = std_sort_algo);

// searching
std::vector<uint32_t> binary_search_algo(
    const std::vector<Entry>& ents,
    const std::string& key);

std::vector<uint32_t> search_memory_index(
    const std::string& tag,
    const std::string& key,
    const std::function<
        std::vector<uint32_t>(const std::vector<Entry>&, const std::string&)
    >& algo = binary_search_algo);

const MemoryIndex* get_memory_index(const std::string& tag);

// misc helpers
bool is_hashnum(const std::string& tok, int& out);

}
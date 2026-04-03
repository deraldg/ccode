// src/cli/cmd_idx.cpp
//
// IDX ON <field|#n> TAG <name> [SORTALGO]
//   - Memory-only sorted index for quick testing/education (no .inx file).
//   - SORTALGO selects the algorithm at runtime (plug-and-play!).
//   - Stored in g_memoryIndexes; search via search_memory_index() in other commands.
//   - Same key processing as 2INX (upper-case + padding for C fields).
//   - Deleted records excluded.

#include "xbase.hpp"
#include "textio.hpp"
#include "cli/index_utils.hpp"   // <-- the abstractions above

#include <iostream>
#include <sstream>
#include <string>

namespace {

bool parse_idx_args(std::istringstream& in, std::string& field, std::string& tag, std::string& algoTok) {
    algoTok = "STD";

    std::string onTok;
    if (!(in >> onTok)) return false;
    if (dottalk::canon(onTok) != "ON") return false;

    if (!(in >> field) || field.empty()) return false;

    std::string tagTok;
    if (!(in >> tagTok)) return false;
    if (dottalk::canon(tagTok) != "TAG") return false;

    if (!(in >> tag) || tag.empty()) return false;

    std::string extra;
    if (in >> extra) {
        algoTok = extra;
        std::string more;
        if (in >> more) return false;
    }
    return true;
}

} // namespace

void cmd_IDX(DbArea& A, std::istringstream& in)
{
    if (!A.isOpen()) { std::cout << "No table open.\n"; return; }

    std::string fieldTok, tag, algoTok;
    if (!parse_idx_args(in, fieldTok, tag, algoTok)) {
        std::cout << "Usage: IDX ON <field|#n> TAG <name> [SORTALGO]\n";
        std::cout << "SORTALGO options: STD (default), BUBBLE\n";
        std::cout << "Examples:\n  IDX ON LNAME TAG students\n  IDX ON #2 TAG students BUBBLE\n";
        return;
    }

    const int fldIdx = dottalk::resolve_field_index(A, fieldTok);
    if (fldIdx < 1) {
        std::cout << "IDX: unknown field '" << fieldTok << "'.\n";
        const auto& Fs = A.fields();
        for (size_t i = 0; i < Fs.size(); ++i) std::cout << "  " << Fs[i].name << "\n";
        return;
    }

    // ==================== PLUGGABLE SORT SELECTION ====================
    std::string algoCanon = dottalk::canon(algoTok);
    std::function<void(std::vector<dottalk::Entry>&)> sorter;
    if (algoCanon == "STD" || algoCanon == "STDSORT") {
        sorter = dottalk::std_sort_algo;
    } else if (algoCanon == "BUBBLE") {
        sorter = dottalk::bubble_sort_algo;
    } else {
        std::cout << "IDX: unknown SORTALGO '" << algoTok << "'. Supported: STD, BUBBLE\n";
        return;
    }

    uint16_t keylen = 0;
    char ftype = 0;
    auto ents = dottalk::create_sorted_entries(A, fldIdx, dottalk::InxFmt::V2_2INX, keylen, ftype, sorter);

    dottalk::MemoryIndex mi{fieldTok, std::move(ents), keylen, ftype, A.recCount()};
    dottalk::g_memoryIndexes[dottalk::canon(tag)] = std::move(mi);

    std::cout << "Memory index created: " << tag
              << "  (sort: " << algoTok << ", expr: " << fieldTok << ", ASC)\n";
}
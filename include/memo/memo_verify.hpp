#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace xbase { class DbArea; }

namespace dottalk::memo {

struct MemoScanResult
{
    int area_slot = -1;
    std::string table_name;

    int x64_memo_fields = 0;

    std::uint64_t records_scanned = 0;
    std::uint64_t memo_refs_found = 0;
    std::uint64_t empty_refs = 0;
    std::uint64_t bad_ids = 0;
    std::uint64_t missing_objects = 0;

    std::uint64_t live_objects = 0;
    std::uint64_t orphan_objects = 0;

    std::uint64_t largest_live_object_bytes = 0;
    std::uint64_t reclaimable_bytes = 0;
    std::uint64_t dtx_append_bytes = 0;

    std::vector<std::uint64_t> referenced_ids;
    std::vector<std::uint64_t> orphan_ids;

    bool ok = true;
    std::string error;
};

bool scan_x64_memos_in_area(xbase::DbArea& area,
                            int area_slot,
                            MemoScanResult& out);

bool gc_orphan_memos_in_area(xbase::DbArea& area,
                             const MemoScanResult& scan,
                             bool confirm,
                             std::uint64_t& removed_objects,
                             std::uint64_t& reclaimed_bytes,
                             std::string& error);

} // namespace dottalk::memo
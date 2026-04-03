#include "memo/memo_verify.hpp"

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "xbase.hpp"
#include "xbase_64.hpp"
#include "memo/memo_auto.hpp"
#include "memo/memostore.hpp"

namespace dottalk::memo {
namespace {

static bool is_x64_memo_field(const xbase::DbArea& area, int field1)
{
    if (field1 < 1 || field1 > area.fieldCount()) return false;
    if (area.versionByte() != xbase::DBF_VERSION_64) return false;

    const auto& f = area.fields()[static_cast<std::size_t>(field1 - 1)];
    return (f.type == 'M' || f.type == 'm') &&
           f.length == xbase::X64_MEMO_FIELD_LEN;
}

static std::uint64_t parse_u64_or_zero(const std::string& s, bool& ok)
{
    ok = true;

    if (s.empty()) return 0;

    try {
        std::size_t used = 0;
        const unsigned long long v = std::stoull(s, &used, 10);
        if (used != s.size()) {
            ok = false;
            return 0;
        }
        return static_cast<std::uint64_t>(v);
    } catch (...) {
        ok = false;
        return 0;
    }
}

static dottalk::memo::MemoStore* memo_store_for_area(xbase::DbArea& area) noexcept
{
    auto* backend = cli_memo::memo_backend_for(area);
    if (!backend) return nullptr;
    return dynamic_cast<dottalk::memo::MemoStore*>(backend);
}

static std::vector<int> collect_x64_memo_fields(const xbase::DbArea& area)
{
    std::vector<int> out;
    if (area.versionByte() != xbase::DBF_VERSION_64) return out;

    for (int i = 1; i <= area.fieldCount(); ++i) {
        if (is_x64_memo_field(area, i))
            out.push_back(i);
    }
    return out;
}

} // namespace

bool scan_x64_memos_in_area(xbase::DbArea& area,
                            int area_slot,
                            MemoScanResult& out)
{
    out = MemoScanResult{};
    out.area_slot = area_slot;

    try {
        out.table_name = area.logicalName().empty() ? area.name() : area.logicalName();
    } catch (...) {
        out.table_name.clear();
    }

    if (!area.isOpen()) {
        out.ok = false;
        out.error = "area is not open";
        return false;
    }

    auto* store = memo_store_for_area(area);
    if (!store) {
        out.ok = false;
        out.error = "memo backend not attached";
        return false;
    }

    const std::vector<int> memo_fields = collect_x64_memo_fields(area);
    out.x64_memo_fields = static_cast<int>(memo_fields.size());

    if (memo_fields.empty()) {
        out.dtx_append_bytes = store->append_bytes();
        out.live_objects = store->live_object_count();
        return true;
    }

    std::set<std::uint64_t> referenced_ids_set;

    const int recs = area.recCount();
    for (int r = 1; r <= recs; ++r) {
        if (!area.gotoRec(r)) continue;
        if (!area.readCurrent()) continue;

        ++out.records_scanned;

        for (int field1 : memo_fields) {
            std::string raw;
            try {
                raw = area.get(field1);
            } catch (...) {
                raw.clear();
            }

            if (raw.empty()) {
                ++out.empty_refs;
                continue;
            }

            bool parse_ok = true;
            const std::uint64_t id = parse_u64_or_zero(raw, parse_ok);

            if (!parse_ok) {
                ++out.bad_ids;
                continue;
            }

            if (id == 0) {
                ++out.empty_refs;
                continue;
            }

            ++out.memo_refs_found;
            referenced_ids_set.insert(id);

            if (!store->exists_id(id)) {
                ++out.missing_objects;
            }
        }
    }

    out.referenced_ids.assign(referenced_ids_set.begin(), referenced_ids_set.end());

    std::vector<std::uint64_t> live_ids;
    if (!store->list_live_ids(live_ids)) {
        out.ok = false;
        out.error = "could not enumerate live memo ids";
        return false;
    }

    out.live_objects = static_cast<std::uint64_t>(live_ids.size());
    out.dtx_append_bytes = store->append_bytes();

    std::set<std::uint64_t> live_set(live_ids.begin(), live_ids.end());

    for (std::uint64_t id : live_ids) {
        std::uint64_t bytes = 0;
        std::string ignored;
        if (store->get_object_size_id(id, bytes, &ignored)) {
            if (bytes > out.largest_live_object_bytes)
                out.largest_live_object_bytes = bytes;
        }

        if (referenced_ids_set.find(id) == referenced_ids_set.end()) {
            out.orphan_ids.push_back(id);
            ++out.orphan_objects;
            out.reclaimable_bytes += bytes;
        }
    }

    out.ok = (out.bad_ids == 0 && out.missing_objects == 0);
    return true;
}

bool gc_orphan_memos_in_area(xbase::DbArea& area,
                             const MemoScanResult& scan,
                             bool confirm,
                             std::uint64_t& removed_objects,
                             std::uint64_t& reclaimed_bytes,
                             std::string& error)
{
    removed_objects = 0;
    reclaimed_bytes = 0;
    error.clear();

    if (!area.isOpen()) {
        error = "area is not open";
        return false;
    }

    auto* store = memo_store_for_area(area);
    if (!store) {
        error = "memo backend not attached";
        return false;
    }

    if (!confirm) {
        reclaimed_bytes = scan.reclaimable_bytes;
        return true;
    }

    for (std::uint64_t id : scan.orphan_ids) {
        std::uint64_t bytes = 0;
        std::string ignored;
        (void)store->get_object_size_id(id, bytes, &ignored);

        if (!store->erase_id(id, &error)) {
            return false;
        }

        ++removed_objects;
        reclaimed_bytes += bytes;
    }

    return true;
}

} // namespace dottalk::memo
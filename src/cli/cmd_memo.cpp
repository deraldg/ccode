#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "memo/memo_verify.hpp"
#include "workareas.hpp"
#include "xbase.hpp"

namespace {

std::string up(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

bool is_open_area(std::size_t slot)
{
    try {
        auto* a = workareas::at(slot);
        return a && a->isOpen();
    } catch (...) {
        return false;
    }
}

std::vector<int> target_slots(bool all_flag)
{
    std::vector<int> out;

    if (!all_flag) {
        out.push_back(static_cast<int>(workareas::current_slot()));
        return out;
    }

    for (std::size_t i = 0; i < workareas::count(); ++i) {
        if (is_open_area(i)) out.push_back(static_cast<int>(i));
    }
    return out;
}

void print_verify(const dottalk::memo::MemoScanResult& s)
{
    std::cout << "MEMO VERIFY\n";
    std::cout << "  area           : " << s.area_slot << "\n";
    std::cout << "  table          : " << s.table_name << "\n";
    std::cout << "  x64 memo fields: " << s.x64_memo_fields << "\n";
    std::cout << "  records scanned: " << s.records_scanned << "\n";
    std::cout << "  memo refs found: " << s.memo_refs_found << "\n";
    std::cout << "  empty refs     : " << s.empty_refs << "\n";
    std::cout << "  bad ids        : " << s.bad_ids << "\n";
    std::cout << "  missing objects: " << s.missing_objects << "\n";
    std::cout << "  orphan objects : " << s.orphan_objects << "\n";

    if (!s.error.empty()) {
        std::cout << "VERIFY: error: " << s.error << "\n";
    } else if (s.ok && s.orphan_objects == 0) {
        std::cout << "VERIFY: OK\n";
    } else if (s.ok) {
        std::cout << "VERIFY: warnings found.\n";
    } else {
        std::cout << "VERIFY: problems found.\n";
    }
}

void print_status(const dottalk::memo::MemoScanResult& s)
{
    std::string status = "OK";
    if (!s.ok) status = "ERROR";
    else if (s.orphan_objects > 0) status = "WARN";

    std::cout << "MEMO STATUS\n";
    std::cout << "  area           : " << s.area_slot << "\n";
    std::cout << "  table          : " << s.table_name << "\n";
    std::cout << "  backend        : DTX\n";
    std::cout << "  x64 memo fields: " << s.x64_memo_fields << "\n";
    std::cout << "  records        : " << s.records_scanned << "\n";
    std::cout << "  memo refs      : " << s.memo_refs_found << "\n";
    std::cout << "  live objects   : " << s.live_objects << "\n";
    std::cout << "  orphan objects : " << s.orphan_objects << "\n";
    std::cout << "  missing refs   : " << s.missing_objects << "\n";
    std::cout << "  empty refs     : " << s.empty_refs << "\n";
    std::cout << "  dtx bytes      : " << s.dtx_append_bytes << "\n";
    std::cout << "  largest memo   : " << s.largest_live_object_bytes << "\n";
    std::cout << "  status         : " << status << "\n";
}

void print_gc(const dottalk::memo::MemoScanResult& s,
              bool confirm,
              std::uint64_t removed_objects,
              std::uint64_t reclaimed_bytes,
              const std::string& error)
{
    std::cout << "MEMO GC\n";
    std::cout << "  area            : " << s.area_slot << "\n";
    std::cout << "  table           : " << s.table_name << "\n";
    std::cout << "  mode            : " << (confirm ? "confirm" : "dry-run") << "\n";
    std::cout << "  orphan objects  : " << s.orphan_objects << "\n";
    std::cout << "  reclaimable     : " << s.reclaimable_bytes << "\n";

    if (confirm) {
        std::cout << "  removed objects : " << removed_objects << "\n";
        std::cout << "  reclaimed bytes : " << reclaimed_bytes << "\n";
    }

    if (!error.empty()) {
        std::cout << "GC: error: " << error << "\n";
    } else if (!confirm) {
        std::cout << "GC: no changes made.\n";
    } else {
        std::cout << "GC complete.\n";
    }
}

} // namespace

void cmd_MEMO(xbase::DbArea& /*unused*/, std::istringstream& iss)
{
    std::string subcmd;
    iss >> subcmd;
    subcmd = up(subcmd);

    bool all_flag = false;
    bool confirm = false;

    std::string tok;
    while (iss >> tok) {
        tok = up(tok);
        if (tok == "ALL") all_flag = true;
        else if (tok == "CONFIRM") confirm = true;
    }

    if (subcmd.empty()) {
        std::cout << "Usage: MEMO STATUS|VERIFY|GC [ALL] [CONFIRM]\n";
        return;
    }

    const std::vector<int> slots = target_slots(all_flag);
    if (slots.empty()) {
        std::cout << "MEMO: no open areas.\n";
        return;
    }

    for (std::size_t i = 0; i < slots.size(); ++i) {
        const int slot = slots[i];
        auto* area = workareas::at(static_cast<std::size_t>(slot));
        if (!area || !area->isOpen()) continue;

        dottalk::memo::MemoScanResult scan;
        if (!dottalk::memo::scan_x64_memos_in_area(*area, slot, scan)) {
            if (subcmd == "VERIFY") print_verify(scan);
            else if (subcmd == "STATUS") print_status(scan);
            else if (subcmd == "GC") {
                std::uint64_t removed = 0;
                std::uint64_t reclaimed = 0;
                print_gc(scan, confirm, removed, reclaimed, scan.error);
            }
            if (i + 1 < slots.size()) std::cout << "\n";
            continue;
        }

        if (subcmd == "VERIFY") {
            print_verify(scan);
        } else if (subcmd == "STATUS") {
            print_status(scan);
        } else if (subcmd == "GC") {
            std::uint64_t removed = 0;
            std::uint64_t reclaimed = 0;
            std::string error;

            if (!dottalk::memo::gc_orphan_memos_in_area(*area,
                                                        scan,
                                                        confirm,
                                                        removed,
                                                        reclaimed,
                                                        error))
            {
                print_gc(scan, confirm, removed, reclaimed, error);
            } else {
                print_gc(scan, confirm, removed, reclaimed, error);
            }
        } else {
            std::cout << "Usage: MEMO STATUS|VERIFY|GC [ALL] [CONFIRM]\n";
            return;
        }

        if (i + 1 < slots.size()) std::cout << "\n";
    }
}
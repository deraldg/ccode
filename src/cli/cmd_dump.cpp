// src/cli/cmd_dump.cpp
// DotTalk++ — DUMP (vertical record view)
// Usage:
//   DUMP                    -> dump all fields, from current rec to EOF
//   DUMP <n>                -> dump first n records from current position
//   DUMP TOP <n>            -> same as above
//   DUMP <field...>         -> dump only listed fields (by name)
//   DUMP TOP <n> <field...> -> limit + field subset
//
// Notes:
// - Reads directly from the DBF file (doesn't mutate engine state).
// - Uses file header/field layout for robust decoding.
// - Does not touch your existing LIST (with color, etc.).

#include "xbase.hpp"
#include "textio.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace xbase;

// --- local helpers (C++-only; no new project deps) ---
static std::string up(const std::string& s) { return textio::up(s); }

static std::string rtrim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n'))
        s.pop_back();
    return s;
}

namespace {
    struct FieldMeta {
        std::string name;
        char type{};
        int length{0};
        int decimal{0};
    };

static bool read_header_and_fields(const DbArea& a, int& cpr, long& data_start,
                                   std::vector<FieldMeta>& metas)
{
    std::ifstream in(a.name(), std::ios::binary);
    if (!in) return false;

    HeaderRec hdr{};
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!in) return false;

    cpr        = hdr.cpr;
    data_start = hdr.data_start;

    metas.clear();
    metas.reserve(128);

    for (;;) {
        // Peek one byte to detect 0x0D terminator safely
        int b = in.peek();
        if (b == EOF) return false;          // truncated header
        if (static_cast<unsigned char>(b) == 0x0D) {
            in.get();                         // consume terminator
            break;
        }

        FieldRec fr{};
        in.read(reinterpret_cast<char*>(&fr), sizeof(fr));
        if (!in) return false;

        char namebuf[12];
        std::memcpy(namebuf, fr.field_name, 11);
        namebuf[11] = '\0';

        FieldMeta m{};
        m.name    = rtrim(std::string(namebuf));
        m.type    = fr.field_type;
        m.length  = static_cast<int>(fr.field_length);
        m.decimal = static_cast<int>(fr.decimal_places);
        metas.push_back(m);

        if (metas.size() > 512) break; // sanity
    }
    return true;
}

    static int find_field_index(const std::vector<FieldMeta>& metas, const std::string& nameUp)
    {
        for (size_t i = 0; i < metas.size(); ++i) {
            if (up(metas[i].name) == nameUp) return static_cast<int>(i);
        }
        return -1;
    }

    static size_t max_name_width(const std::vector<FieldMeta>& metas, const std::vector<int>& wantIdx)
    {
        size_t w = 0;
        if (!wantIdx.empty()) {
            for (int idx : wantIdx) w = std::max(w, metas[idx].name.size());
        } else {
            for (auto& m : metas) w = std::max(w, m.name.size());
        }
        return w;
    }

    static void print_record_vertical(std::istream& in, std::streampos recPos, int cpr,
                                      const std::vector<FieldMeta>& metas,
                                      const std::vector<int>& wantIdx,
                                      size_t labelW, long recno, bool showDelFlag)
    {
        in.seekg(recPos, std::ios::beg);
        char delFlag = ' ';
        in.read(&delFlag, 1);
        if (!in) return;

        std::string row;
        row.resize(static_cast<size_t>(cpr - 1), ' ');
        in.read(&row[0], cpr - 1);
        if (!in) return;

        // header line per record
        if (showDelFlag && delFlag == IS_DELETED)
            std::cout << "Record " << recno << " [DELETED]\n";
        else
            std::cout << "Record " << recno << "\n";

        size_t offset = 0;
        for (size_t mi = 0; mi < metas.size(); ++mi) {
            const auto& m = metas[mi];
            std::string val = row.substr(offset, static_cast<size_t>(m.length));
            val = rtrim(val);
            offset += static_cast<size_t>(m.length);

            if (!wantIdx.empty()) {
                // print only requested fields
                if (std::find(wantIdx.begin(), wantIdx.end(), static_cast<int>(mi)) == wantIdx.end())
                    continue;
            }

            // NAME padding to align colons
            std::cout << m.name;
            if (m.name.size() < labelW) std::cout << std::string(labelW - m.name.size(), ' ');
            std::cout << " : " << val << "\n";
        }
        std::cout << "\n";
    }
} // anon

void cmd_DUMP(DbArea& a, std::istringstream& iss)
{
    if (!a.isOpen()) { std::cout << "No table open.\n"; return; }

    // Parse args
    std::vector<std::string> args;
    { std::string tok; while (iss >> tok) args.push_back(tok); }

    long limit = -1; // unlimited
    size_t argPos = 0;

    if (argPos < args.size() && up(args[argPos]) == "TOP") {
        if (argPos + 1 < args.size()) {
            limit = std::max(0L, std::stol(args[argPos + 1]));
            argPos += 2;
        } else {
            std::cout << "Usage: DUMP TOP <n> [field ...]\n";
            return;
        }
    } else if (argPos < args.size()) {
        const std::string& a0 = args[argPos];
        const bool allDigits = !a0.empty() &&
            std::all_of(a0.begin(), a0.end(), [](unsigned char c){ return std::isdigit(c); });
        if (allDigits) {
            limit = std::max(0L, std::stol(a0));
            ++argPos;
        }
    }

    int cpr = 0; long data_start = 0;
    std::vector<FieldMeta> metas;
    if (!read_header_and_fields(a, cpr, data_start, metas)) {
        std::cout << "Failed to read header\n"; return;
    }

    // Requested field subset
    std::vector<int> wantIdx;
    for (; argPos < args.size(); ++argPos) {
        int idx = find_field_index(metas, up(args[argPos]));
        if (idx >= 0) wantIdx.push_back(idx);
        else std::cout << "Warning: unknown field '" << args[argPos] << "'\n";
    }

    // label alignment width
    const size_t labelW = max_name_width(metas, wantIdx);

    std::ifstream in(a.name(), std::ios::binary);
    if (!in) { std::cout << "Open failed: Failed to read header\n"; return; }

    const long total = a.recCount();
    long startRec = std::max<long>(1, a.recno() ? a.recno() : 1);
    long shown = 0;

    for (long r = startRec; r <= total; ++r) {
        if (limit >= 0 && shown >= limit) break;
        std::streampos recPos = static_cast<std::streampos>(data_start)
                              + static_cast<std::streamoff>((r - 1) * cpr);
        print_record_vertical(in, recPos, cpr, metas, wantIdx, labelW, r, /*showDelFlag=*/true);
        ++shown;
    }
}

// src/cli/cmd_replace.cpp
// DotTalk++ — REPLACE <field> WITH <value>
// Edits the CURRENT record (RECNO()) in place.
// Types supported: C, N, D, L. Values are padded/justified per field width/decimals.

#include "xbase.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <iomanip>

using namespace xbase;

namespace {
    struct FieldMeta {
        std::string name;
        char type{};
        int length{0};
        int decimal{0};
        size_t offset{0}; // byte offset of this field inside record (after delete flag)
    };

    static std::string up(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c){ return std::toupper(c); });
        return s;
    }
    static std::string trim(std::string s) {
        auto issp = [](unsigned char c){ return c==' '||c=='\t'||c=='\r'||c=='\n'; };
        while (!s.empty() && issp(s.front())) s.erase(s.begin());
        while (!s.empty() && issp(s.back()))  s.pop_back();
        return s;
    }
    static bool read_header_and_fields(const DbArea& a, HeaderRec& hdr, std::vector<FieldMeta>& metas) {
        std::ifstream in(a.name(), std::ios::binary);
        if (!in) return false;
        in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        if (!in) return false;

        metas.clear();
        size_t offset = 0;
        for (;;) {
            FieldRec fr{};
            in.read(reinterpret_cast<char*>(&fr), sizeof(fr));
            if (!in) return false;
            if (static_cast<unsigned char>(fr.field_name[0]) == 0x0D) break;

            char namebuf[12]; std::memcpy(namebuf, fr.field_name, 11); namebuf[11] = '\0';
            FieldMeta m;
            m.name   = trim(std::string(namebuf));
            m.type   = fr.field_type;
            m.length = static_cast<int>(fr.field_length);
            m.decimal= static_cast<int>(fr.decimal_places);
            m.offset = offset;
            metas.push_back(m);

            offset += static_cast<size_t>(m.length);
            if (metas.size() > 512) break;
        }
        return true;
    }
    static int find_field(const std::vector<FieldMeta>& metas, const std::string& nameUp) {
        for (size_t i=0;i<metas.size();++i)
            if (up(metas[i].name) == nameUp) return int(i);
        return -1;
    }
    // format value buffer of exact width, per type
    static std::string format_value(const FieldMeta& m, const std::string& raw, bool& ok) {
        ok = true;
        std::string out(m.length, ' ');

        switch (m.type) {
        case 'C': { // left-justified, space padded
            std::string v = raw;
            // strip surrounding quotes if present
            if (v.size()>=2 && ((v.front()=='"' && v.back()=='"') || (v.front()=='\'' && v.back()=='\'')))
                v = v.substr(1, v.size()-2);
            v = trim(v);
            if (v.size() > size_t(m.length)) v.resize(size_t(m.length));
            std::copy(v.begin(), v.end(), out.begin());
            break;
        }
        case 'N': { // right-justified numeric; respect decimals
            // accept plain numbers; format into width with decimals
            try {
                double d = std::stod(raw);
                std::ostringstream oss;
                if (m.decimal > 0) {
                    oss << std::fixed << std::setprecision(m.decimal) << d;
                } else {
                    // avoid scientific; cast to long long when safe
                    long long ll = static_cast<long long>(std::llround(d));
                    oss << ll;
                }
                std::string v = oss.str();
                if (v.size() > size_t(m.length)) {
                    // too big to fit
                    ok = false; return out;
                }
                // right-align inside width
                std::copy(v.rbegin(), v.rend(), out.rbegin());
            } catch (...) { ok = false; }
            break;
        }
        case 'L': { // Logical: T/F
            char c = std::toupper(raw.empty() ? ' ' : raw[0]);
            if (c=='1'||c=='Y'||c=='T') c='T';
            else if (c=='0'||c=='N'||c=='F') c='F';
            else { ok=false; break; }
            out[0] = c;
            break;
        }
        case 'D': { // Date: YYYYMMDD exact 8 chars
            std::string v = raw;
            if (v.size()>=2 && ((v.front()=='"' && v.back()=='"') || (v.front()=='\'' && v.back()=='\'')))
                v = v.substr(1, v.size()-2);
            v = trim(v);
            if (v.size() != 8 || !std::all_of(v.begin(), v.end(), ::isdigit)) { ok=false; break; }
            std::copy(v.begin(), v.end(), out.begin()); // width should be 8
            break;
        }
        default:
            ok = false; // unsupported type for now
            break;
        }
        return out;
    }
} // ns

void cmd_REPLACE(DbArea& a, std::istringstream& iss)
{
    if (!a.isOpen()) { std::cout << "No table open.\n"; return; }
    if (a.recno() <= 0 || a.recno() > a.recCount()) { std::cout << "Invalid current record.\n"; return; }

    // Parse: REPLACE <field> [WITH] <value...>
    std::string field; if (!(iss >> field)) { std::cout << "Usage: REPLACE <field> WITH <value>\n"; return; }

    // optional WITH
    std::string maybeWith; std::streampos afterFieldPos = iss.tellg();
    if (iss >> maybeWith) {
        if (up(maybeWith) != "WITH") {
            // not WITH; rewind to before it to include in value
            iss.clear(); iss.seekg(afterFieldPos);
        }
    }
    std::string rest; std::getline(iss, rest);
    std::string value = trim(rest);
    if (value.empty()) { std::cout << "Usage: REPLACE <field> WITH <value>\n"; return; }

    HeaderRec hdr{}; std::vector<FieldMeta> metas;
    if (!read_header_and_fields(a, hdr, metas)) { std::cout << "Failed to read header\n"; return; }

    int fi = find_field(metas, up(field));
    if (fi < 0) { std::cout << "Unknown field: " << field << "\n"; return; }
    const auto& m = metas[fi];

    bool ok = false;
    std::string cell = format_value(m, value, ok);
    if (!ok) {
        std::cout << "Value not valid for field " << m.name << " (type " << m.type << ", len "
                  << m.length << (m.decimal? ("," + std::to_string(m.decimal)) : std::string()) << ")\n";
        return;
    }

    // open file for in-place write
    std::fstream io(a.name(), std::ios::in | std::ios::out | std::ios::binary);
    if (!io) { std::cout << "Open failed: cannot write file\n"; return; }

    // position at start of current record's data (skip delete flag)
    const long rec = a.recno();
    std::streampos pos = static_cast<std::streampos>(hdr.data_start)
                       + static_cast<std::streamoff>((rec - 1) * hdr.cpr)
                       + std::streamoff(1)     // delete flag
                       + static_cast<std::streamoff>(m.offset);

    io.seekp(pos, std::ios::beg);
    io.write(cell.data(), static_cast<std::streamsize>(cell.size()));
    io.flush();
    if (!io) { std::cout << "Write failed.\n"; return; }

    std::cout << "Replaced " << m.name << " at recno " << rec << ".\n";
    /* NEW: refresh the engine's field buffer so DISPLAY shows the new values */
    try {
        a.gotoRec(rec);  // re-read current record into in-memory buffers
    } catch (...) {
    // ignore – DISPLAY/DUMP will still show on-disk content
}
}

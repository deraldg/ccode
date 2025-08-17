// DotTalk++ — APPEND_BLANK [n]
// Appends n blank records (delete flag=' ', body=' ') without prompting.

#include "xbase.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <cstdint>

using namespace xbase;

static uint16_t rd_le16(const unsigned char* p){ return uint16_t(p[0] | (p[1]<<8)); }
static uint32_t rd_le32(const unsigned char* p){ return uint32_t(p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24)); }
static void wr_le32(unsigned char* p, uint32_t v){ p[0]=v&0xFF; p[1]=(v>>8)&0xFF; p[2]=(v>>16)&0xFF; p[3]=(v>>24)&0xFF; }

void cmd_APPEND_BLANK(xbase::DbArea& a, std::istringstream& iss)
{
    if (!a.isOpen()) { std::cout << "No table open.\n"; return; }

    long n = 1;
    if (!(iss >> n)) n = 1;
    if (n <= 0) { std::cout << "Usage: APPEND_BLANK [n]\n"; return; }

    std::fstream io(a.name(), std::ios::in | std::ios::out | std::ios::binary);
    if (!io) { std::cout << "Open failed: cannot write file\n"; return; }

    unsigned char hdr[32] = {0};
    io.read(reinterpret_cast<char*>(hdr), 32);
    if (!io) { std::cout << "Failed to read header\n"; return; }

    const uint16_t header_len = rd_le16(&hdr[8]);   // == data_start
    const uint16_t rec_len    = rd_le16(&hdr[10]);  // == cpr
    uint32_t old_count        = rd_le32(&hdr[4]);
    uint32_t new_count        = old_count;

    std::vector<char> rec(rec_len, ' ');
    rec[0] = ' '; // not deleted

    for (long i = 0; i < n; ++i) {
        std::streampos pos = std::streampos(header_len) + std::streamoff((new_count + i) * rec_len);
        io.seekp(pos, std::ios::beg);
        io.write(rec.data(), static_cast<std::streamsize>(rec.size()));
        if (!io) { std::cout << "Write failed\n"; return; }
    }

    // bump count in header
    new_count += static_cast<uint32_t>(n);
    io.seekp(4, std::ios::beg);
    unsigned char tmp[4]; wr_le32(tmp, new_count);
    io.write(reinterpret_cast<const char*>(tmp), 4);

    // EOF marker after last record
    std::streampos eof_pos = std::streampos(header_len) + std::streamoff(new_count * rec_len);
    io.seekp(eof_pos, std::ios::beg);
    const unsigned char eof = 0x1A;
    io.write(reinterpret_cast<const char*>(&eof), 1);
    io.flush();
    if (!io) { std::cout << "Append failed (flush)\n"; return; }

    std::cout << "Appended " << n << " blank record(s). New count: " << new_count << ".\n";

    // 🔁 Refresh the engine’s view and move to the first newly-added record
    try {
        const std::string fname = a.name();
        a.open(fname);  // re-open to reload header/recCount/cpr
        long firstNew = static_cast<long>(old_count) + 1;
        if (new_count >= static_cast<uint32_t>(firstNew)) {
            a.gotoRec(firstNew);   // position so REPLACE/EDIT works immediately
        } else if (new_count > 0) {
            a.gotoRec(1);
        }
    } catch (const std::exception& e) {
        std::cout << "Note: refresh failed: " << e.what() << "\n";
    }
}

#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <vector>
#include <cstdio>   // std::remove, std::rename
#include "command_registry.hpp"
#include "xbase.hpp"

// Robust PACK implementation with external linkage for cmd_PACK:
// - Reads original header + field descriptors
// - Writes new file with ONLY non-deleted (' ') records
// - Updates header.num_of_recs to kept count
// - Preserves header/data_start/cpr and writes 0x0D terminator
// - Replaces the original file atomically (best effort on Windows)

static bool read_header_and_fields(std::ifstream& in, xbase::HeaderRec& hdr,
                                   std::vector<xbase::FieldRec>& fields)
{
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!in) return false;

    // Field descriptor array ends with 0x0D
    while (true) {
        xbase::FieldRec fr{};
        in.read(reinterpret_cast<char*>(&fr), sizeof(fr));
        if (!in) return false;

        // Field terminator per dBASE spec: 0x0D
        if (static_cast<unsigned char>(fr.field_name[0]) == 0x0D) {
            break;
        }
        fields.push_back(fr);
    }
    return true;
}

static bool write_header_and_fields(std::ofstream& out, const xbase::HeaderRec& hdr,
                                    const std::vector<xbase::FieldRec>& fields)
{
    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    if (!out) return false;
    for (const auto& fr : fields) {
        out.write(reinterpret_cast<const char*>(&fr), sizeof(fr));
        if (!out) return false;
    }
    // Terminator
    unsigned char term = xbase::HEADER_TERM_BYTE;
    out.write(reinterpret_cast<const char*>(&term), 1);
    return static_cast<bool>(out);
}

// IMPORTANT: cmd_PACK must have external linkage because shell references it directly.
void cmd_PACK(xbase::DbArea& area, std::istringstream& iss)
{
    (void)iss;
    if (!area.isOpen()) {
        std::cout << "No table is open. Use USE <file> first.\n";
        return;
    }

    // Original file path
    const std::string dbfPath = xbase::dbNameWithExt(area.name());
    const std::string tmpPath = dbfPath + ".pack_tmp";

    // Open input
    std::ifstream in(dbfPath, std::ios::binary);
    if (!in) {
        std::cout << "PACK: failed to open input: " << dbfPath << "\n";
        return;
    }

    // Read header + field descriptors
    xbase::HeaderRec hdr{};
    std::vector<xbase::FieldRec> fields;
    if (!read_header_and_fields(in, hdr, fields)) {
        std::cout << "PACK: failed to read DBF header/fields.\n";
        return;
    }

    // Open output
    std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cout << "PACK: failed to open output: " << tmpPath << "\n";
        return;
    }

    // We'll correct hdr.num_of_recs after copying; keep a working copy
    xbase::HeaderRec outHdr = hdr;
    // Write header + fields + terminator first; we'll append records after
    if (!write_header_and_fields(out, outHdr, fields)) {
        std::cout << "PACK: failed to write output header/fields.\n";
        return;
    }

    // Ensure input stream is at data start (hdr.data_start from header)
    in.clear();
    in.seekg(static_cast<std::streamoff>(hdr.data_start), std::ios::beg);
    if (!in) {
        std::cout << "PACK: seek to data_start failed.\n";
        return;
    }

    // Copy only non-deleted records
    std::vector<char> buf(hdr.cpr);
    int kept = 0;
    for (int r = 1; r <= hdr.num_of_recs; ++r) {
        in.read(buf.data(), buf.size());
        if (!in) break;

        const char delFlag = buf[0];
        if (delFlag != xbase::IS_DELETED) {
            buf[0] = xbase::NOT_DELETED; // normalize
            out.write(buf.data(), buf.size());
            if (!out) { std::cout << "PACK: write failed.\n"; return; }
            ++kept;
        }
    }

    out.flush();
    if (!out) { std::cout << "PACK: flush failed.\n"; return; }

    // Seek back and patch the kept count into header.num_of_recs
    out.seekp(0, std::ios::beg);
    outHdr.num_of_recs = kept;
    out.write(reinterpret_cast<const char*>(&outHdr), sizeof(outHdr));
    out.flush();

    out.close();
    in.close();

    // Replace original file (best effort; on Windows, remove first then rename)
    std::remove((dbfPath + ".bak").c_str()); // cleanup any prior backup
    if (std::rename(dbfPath.c_str(), (dbfPath + ".bak").c_str()) != 0) {
        // If rename-to-backup fails, attempt direct removal to allow replace
        std::remove(dbfPath.c_str());
    }
    if (std::rename(tmpPath.c_str(), dbfPath.c_str()) != 0) {
        std::cout << "PACK: failed to replace original file. Temporary file left at: " << tmpPath << "\n";
        return;
    }

    std::cout << "PACK complete. Kept " << kept << " of " << hdr.num_of_recs << " records.\n";
    std::cout << "Backup saved as " << dbfPath << ".bak\n";
}

// Also self-register with the command registry for consistency.
static bool s_registered = [](){
    static cli::CommandRegistry reg;
    reg.add("PACK", &cmd_PACK);
    return true;
}();

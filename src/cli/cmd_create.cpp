// src/cli/cmd_create.cpp
// DotTalk++ — CREATE <name> (<field specs...>)
// Example:
//   CREATE students (STUDENT_ID N 5, FIRST_NAME C 20, LAST_NAME C 20, DOB D, GPA N 4 2, IS_ACTIVE L)

#include "xbase.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <ctime>

using namespace xbase;

namespace {

struct FieldSpec {
    std::string name;
    char type {};      // 'C','N','D','L'
    int length {0};    // width
    int decimal {0};   // decimals (N only)
};

static std::string up(std::string s) {
    for (auto& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}
static std::string trim(std::string s) {
    auto issp = [](unsigned char c){ return c==' '||c=='\t'||c=='\r'||c=='\n'; };
    while (!s.empty() && issp(s.front())) s.erase(s.begin());
    while (!s.empty() && issp(s.back()))  s.pop_back();
    return s;
}

static bool parse_field_specs(const std::string& in, std::vector<FieldSpec>& out, std::string& err) {
    // very simple tokenizer: split on space/comma/paren
    std::vector<std::string> tok;
    tok.reserve(128);
    std::string cur;
    auto flush = [&](){
        if (!cur.empty()) { tok.push_back(cur); cur.clear(); }
    };
    for (char ch : in) {
        if (ch==',' || ch=='(' || ch==')') { flush(); continue; }
        if (std::isspace(static_cast<unsigned char>(ch))) { flush(); continue; }
        cur.push_back(ch);
    }
    flush();

    // Expect triplets/quads: NAME TYPE [LEN [DEC]] depending on TYPE
    out.clear();
    for (size_t i=0; i<tok.size();) {
        if (i >= tok.size()) break;
        FieldSpec fs{};
        fs.name = up(tok[i++]);
        if (i >= tok.size()) { err = "Missing TYPE for field " + fs.name; return false; }
        std::string t = up(tok[i++]);
        if (t.size()!=1 || (t[0]!='C' && t[0]!='N' && t[0]!='D' && t[0]!='L')) {
            err = "Invalid TYPE for field " + fs.name + " (use C,N,D,L)"; return false;
        }
        fs.type = t[0];

        if (fs.type=='C') {
            if (i >= tok.size()) { err="Missing length for C field " + fs.name; return false; }
            fs.length = std::stoi(tok[i++]);
            if (fs.length <= 0 || fs.length > 254) { err="Length out of range for " + fs.name; return false; }
        } else if (fs.type=='N') {
            if (i >= tok.size()) { err="Missing length for N field " + fs.name; return false; }
            fs.length = std::stoi(tok[i++]);
            if (fs.length <= 0 || fs.length > 254) { err="Length out of range for " + fs.name; return false; }
            if (i < tok.size()) {
                // optional decimals
                bool digits = std::all_of(tok[i].begin(), tok[i].end(), [](unsigned char c){ return std::isdigit(c); });
                if (digits) {
                    fs.decimal = std::stoi(tok[i++]);
                    if (fs.decimal < 0 || fs.decimal >= fs.length) { err="Invalid decimals for " + fs.name; return false; }
                }
            }
        } else if (fs.type=='D') {
            fs.length = 8;
        } else if (fs.type=='L') {
            fs.length = 1;
        }
        out.push_back(fs);
    }
    if (out.empty()) { err = "No fields specified."; return false; }
    return true;
}

// Write a minimal dBase III header + field descriptors
static bool write_dbf(const std::string& path, const std::vector<FieldSpec>& fields, std::string& err) {
    // record length = 1 (delete flag) + sum(lengths)
    int recLen = 1;
    for (auto& f : fields) recLen += f.length;

    // header length = 32 (header) + 32 * field_count + 1 (0x0D terminator)
    int hdrLen = 32 + 32 * static_cast<int>(fields.size()) + 1;

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) { err = "Cannot create file"; return false; }

    // dBase III header layout
    unsigned char header[32] = {0};
    header[0] = 0x03; // version

    // YY MM DD
    std::time_t now = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    tm = *std::localtime(&now);
#endif
    header[1] = static_cast<unsigned char>((tm.tm_year % 100));
    header[2] = static_cast<unsigned char>(tm.tm_mon + 1);
    header[3] = static_cast<unsigned char>(tm.tm_mday);

    // number of records (4 bytes LE) => 0 initially
    // header length (2 bytes LE)
    header[8]  = static_cast<unsigned char>(hdrLen & 0xFF);
    header[9]  = static_cast<unsigned char>((hdrLen >> 8) & 0xFF);

    // record length (2 bytes LE)
    header[10] = static_cast<unsigned char>(recLen & 0xFF);
    header[11] = static_cast<unsigned char>((recLen >> 8) & 0xFF);

    out.write(reinterpret_cast<const char*>(header), 32);

    // field descriptors (32 bytes each)
    int offset = 1; // first byte of record is delete flag
    for (auto& f : fields) {
        unsigned char fd[32] = {0};
        // name (up to 11, null padded)
        for (size_t i=0; i<f.name.size() && i<11; ++i) fd[i] = static_cast<unsigned char>(f.name[i]);

        fd[11] = static_cast<unsigned char>(f.type); // type

        // field data address (4 bytes) — not required by many readers; leave 0
        // length / decimal
        fd[16] = static_cast<unsigned char>(f.length);
        fd[17] = static_cast<unsigned char>(std::max(0, f.decimal));

        // (20-31) reserved zeros

        out.write(reinterpret_cast<const char*>(fd), 32);
        offset += f.length;
    }

    // field descriptor terminator 0x0D
    unsigned char term = 0x0D;
    out.write(reinterpret_cast<const char*>(&term), 1);

    // optional EOF marker 0x1A (common; harmless)
    unsigned char eof = 0x1A;
    out.write(reinterpret_cast<const char*>(&eof), 1);

    out.flush();
    if (!out) { err = "Write failed"; return false; }
    return true;
}

} // ns

void cmd_CREATE(DbArea& a, std::istringstream& iss)
{
    std::string name;
    if (!(iss >> name)) {
        std::cout << "Usage: CREATE <name> (<field specs>)\n";
        return;
    }

    // collect the rest of the line as the field spec string
    std::string rest; std::getline(iss, rest);
    rest = trim(rest);
    if (rest.empty()) {
        std::cout << "Usage: CREATE <name> (FIELD1 C 10, FIELD2 N 8 2, DOB D, ACTIVE L)\n";
        return;
    }

    // ensure .dbf suffix
    if (name.size() < 4 || up(name.substr(name.size()-4)) != ".DBF") name += ".dbf";

    std::vector<FieldSpec> fields;
    std::string perr;
    if (!parse_field_specs(rest, fields, perr)) {
        std::cout << "CREATE error: " << perr << "\n";
        return;
    }

    std::string werr;
    if (!write_dbf(name, fields, werr)) {
        std::cout << "CREATE failed: " << werr << "\n";
        return;
    }

    std::cout << "Created " << name << " with " << fields.size() << " field(s).\n";

    // Try opening it so user can start editing right away
    try {
        a.open(name);
        std::cout << "Opened " << name << " with " << a.recCount() << " records.\n";
    } catch (...) {
        // If your USE command has extra behaviors (index, etc.), user can call USE manually.
    }
}

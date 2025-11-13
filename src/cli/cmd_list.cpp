// src/cli/cmd_list.cpp
#include "xbase.hpp"
#include "predicates.hpp"
#include "textio.hpp"
#include "order_state.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <limits>
#include <cctype>
#include <algorithm>
#include <sstream>
#include <vector>
#include <fstream>
#include <filesystem>

namespace {

// ANSI helpers
constexpr const char* RESET    = "\x1b[0m";
constexpr const char* BG_AMBER = "\x1b[48;5;214m"; // fallback: "\x1b[43m"
constexpr const char* BG_RED   = "\x1b[41m";
constexpr const char* FG_WHITE = "\x1b[97m";

//constexpr const char* save_color ;

inline bool is_uint(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(),
        [](unsigned char c){ return std::isdigit(c)!=0; });
}

struct Options {
    bool all{false};            // LIST ALL (show deleted too)
    int  limit{20};             // default page size
    bool haveFilter{false};     // LIST [N|ALL] FOR <fld> <op> <value...>
    std::string fld, op, val;
};

Options parse_opts(std::istringstream& iss) {
    Options o{};
    std::string tok;

    // Optional first token: ALL or number
    std::streampos save = iss.tellg();
    if (iss >> tok) {
        if (textio::ieq(tok, "ALL")) {
            o.all = true;
        } else if (is_uint(tok)) {
            o.limit = std::max(0, std::stoi(tok));
        } else {
            iss.clear();
            iss.seekg(save);
        }
    }

    // Optional: FOR <fld> <op> <value...>
    save = iss.tellg();
    std::string forWord;
    if (iss >> forWord) {
        if (textio::ieq(forWord, "FOR")) {
            if (iss >> o.fld >> o.op) {
                std::string rest;
                std::getline(iss, rest);
                o.val = textio::trim(rest);
                o.haveFilter = !o.fld.empty() && !o.op.empty() && !o.val.empty();
            }
        } else {
            iss.clear();
            iss.seekg(save);
        }
    }
    return o;
}

// width = digits(recCount), min 3 so small tables still look nice
int recno_width(const xbase::DbArea& a) {
    int n = std::max(1, a.recCount());
    int w = 0;
    while (n) { n /= 10; ++w; }
    return std::max(3, w);
}

inline void print_del_flag(bool isDel) {
    if (isDel) {
        // deleted: red background + white '*'
        std::cout << BG_RED << FG_WHITE << '*' << RESET;
    } else {
        // not deleted: amber background space
        std::cout << BG_AMBER << ' ' << RESET;
    }
}

void print_header(const xbase::DbArea& a, int recw) {
    const auto& Fs = a.fields();
    // Columns: [status(1)] [space] [recno(recw)] [space] [fields...]
    std::cout << ' ' << ' ' << std::setw(recw) << "" << " ";
    for (const auto& f : Fs) {
        std::cout << std::left << std::setw(static_cast<int>(f.length)) << f.name << " ";
    }
    std::cout << std::right << "\n";
}

void print_row(const xbase::DbArea& a, int recw) {
    const auto& Fs = a.fields();
    print_del_flag(a.isDeleted());
    std::cout << " " << std::setw(recw) << a.recno() << " ";
    for (int i = 1; i <= static_cast<int>(Fs.size()); ++i) {
        std::string s = a.get(i);
        int w = static_cast<int>(Fs[static_cast<size_t>(i-1)].length);
        if (static_cast<int>(s.size()) > w) s.resize(static_cast<size_t>(w));
        std::cout << std::left << std::setw(w) << s << " ";
    }
    std::cout << std::right << "\n";
}

// ---- helpers to read little-endian ints safely from a stream ----
static bool rd_u16(std::istream& in, uint16_t& v) {
    unsigned char b[2];
    if (!in.read(reinterpret_cast<char*>(b), 2)) return false;
    v = static_cast<uint16_t>(b[0] | (b[1] << 8));
    return true;
}
static bool rd_u32(std::istream& in, uint32_t& v) {
    unsigned char b[4];
    if (!in.read(reinterpret_cast<char*>(b), 4)) return false;
    v = static_cast<uint32_t>(b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24));
    return true;
}

// Try multiple index encodings until one succeeds.
bool load_inx_recnos(const std::string& path, int32_t maxRecno, std::vector<uint32_t>& out, std::string* err) {
    namespace fs = std::filesystem;
    out.clear();

    std::ifstream f(path, std::ios::binary);
    if (!f) { if (err) *err = "cannot open"; return false; }

    auto valid_rn = [&](uint32_t rn){ return rn >= 1u && rn <= static_cast<uint32_t>(maxRecno); };

    // -------------- Try v1: "1INX" magic --------------
    {
        f.clear(); f.seekg(0);
        char magic[4]{};
        if (f.read(magic, 4)) {
            if (std::string(magic, 4) == "1INX") {
                uint16_t skip=0, nameLen=0;
                if (!rd_u16(f, skip) || !rd_u16(f, nameLen)) { if (err) *err = "short header"; return false; }
                if (nameLen > 8192) { if (err) *err = "name too long"; return false; }
                f.seekg(nameLen, std::ios::cur);
                uint32_t count=0;
                if (!rd_u32(f, count)) { if (err) *err = "short count"; return false; }

                out.reserve(count);
                for (uint32_t i = 0; i < count; ++i) {
                    uint16_t klen=0;
                    if (!rd_u16(f, klen)) { if (err) *err = "short entry (klen)"; return false; }
                    f.seekg(klen, std::ios::cur);
                    uint32_t rn=0;
                    if (!rd_u32(f, rn)) { if (err) *err = "short entry (recno)"; return false; }
                    if (!valid_rn(rn)) continue;
                    out.push_back(rn);
                }
                if (!out.empty()) return true;
                // if empty, try other formats before failing
            }
        }
    }

    // -------------- Try v0a: [u16 nameLen][name][u32 count][(u16 klen)(key)(u32 rn)] --------------
    {
        f.clear(); f.seekg(0);
        uint16_t nameLen=0;
        if (rd_u16(f, nameLen) && nameLen <= 8192) {
            f.seekg(nameLen, std::ios::cur);
            uint32_t count=0;
            if (rd_u32(f, count)) {
                std::vector<uint32_t> tmp; tmp.reserve(count);
                bool ok=true;
                for (uint32_t i=0; i<count; ++i) {
                    uint16_t klen=0; uint32_t rn=0;
                    if (!rd_u16(f, klen)) { ok=false; break; }
                    f.seekg(klen, std::ios::cur);
                    if (!rd_u32(f, rn)) { ok=false; break; }
                    if (!valid_rn(rn)) continue;
                    tmp.push_back(rn);
                }
                if (ok && !tmp.empty()) { out.swap(tmp); return true; }
            }
        }
    }

    // -------------- Try v0b: [u32 count][(u16 klen)(key)(u32 rn)] --------------
    {
        f.clear(); f.seekg(0);
        uint32_t count=0;
        if (rd_u32(f, count) && count < 10'000'000u) {
            std::vector<uint32_t> tmp; tmp.reserve(count);
            bool ok=true;
            for (uint32_t i=0; i<count; ++i) {
                uint16_t klen=0; uint32_t rn=0;
                if (!rd_u16(f, klen)) { ok=false; break; }
                f.seekg(klen, std::ios::cur);
                if (!rd_u32(f, rn)) { ok=false; break; }
                if (!valid_rn(rn)) continue;
                tmp.push_back(rn);
            }
            if (ok && !tmp.empty()) { out.swap(tmp); return true; }
        }
    }

    // -------------- Try v0c: [u32 count][(u32 rn)(u16 klen)(key)] --------------
    {
        f.clear(); f.seekg(0);
        uint32_t count=0;
        if (rd_u32(f, count) && count < 10'000'000u) {
            std::vector<uint32_t> tmp; tmp.reserve(count);
            bool ok=true;
            for (uint32_t i=0; i<count; ++i) {
                uint32_t rn=0; uint16_t klen=0;
                if (!rd_u32(f, rn)) { ok=false; break; }
                if (!rd_u16(f, klen)) { ok=false; break; }
                f.seekg(klen, std::ios::cur);
                if (!valid_rn(rn)) continue;
                tmp.push_back(rn);
            }
            if (ok && !tmp.empty()) { out.swap(tmp); return true; }
        }
    }

    if (err) *err = "unrecognized index format";
    return false;
}

} // namespace

// Shell entrypoint
void cmd_LIST(xbase::DbArea& a, std::istringstream& iss) {
    if (!a.isOpen()) { std::cout << "No table open.\n"; return; }

    Options opt = parse_opts(iss);
    const int32_t total = a.recCount();
    if (total <= 0) { std::cout << "(empty)\n"; return; }

    // If ALL, always start from the top; else from current (or top if unset)
    if (opt.all) a.top(); else if (a.recno() <= 0) a.top();

    const int recw = recno_width(a);
    print_header(a, recw);

    // If an index is active, list rows in its order and return.
    if (orderstate::hasOrder(a)) {
        const std::string inxPath = orderstate::orderName(a);

        std::vector<uint32_t> recnos;
        std::string err;
        if (!load_inx_recnos(inxPath, a.recCount(), recnos, &err)) {
            std::cout << "Failed to open index: " << inxPath
                      << " (" << err << ")\n";
            // fall through to physical order
        } else {
            const bool asc = orderstate::isAscending(a); // ← NEW: honor ASCEND/DESCEND
            int printed = 0;

            if (asc) {
                for (uint32_t rn : recnos) {
                    if (!a.gotoRec(static_cast<int32_t>(rn))) continue;
                    if (!a.readCurrent()) continue;
                    if (a.isDeleted() && !opt.all) continue;
                    if (opt.haveFilter && !predicates::eval(a, opt.fld, opt.op, opt.val))
                        continue;

                    print_row(a, recw);
                    ++printed;
                    if (!opt.all && opt.limit > 0 && printed >= opt.limit) break;
                }
            } else {
                for (auto it = recnos.rbegin(); it != recnos.rend(); ++it) {
                    uint32_t rn = *it;
                    if (!a.gotoRec(static_cast<int32_t>(rn))) continue;
                    if (!a.readCurrent()) continue;
                    if (a.isDeleted() && !opt.all) continue;
                    if (opt.haveFilter && !predicates::eval(a, opt.fld, opt.op, opt.val))
                        continue;

                    print_row(a, recw);
                    ++printed;
                    if (!opt.all && opt.limit > 0 && printed >= opt.limit) break;
                }
            }

            if (!opt.all) {
                std::cout << printed << " record(s) listed (limit "
                          << opt.limit << "). Use LIST ALL to show more.\n";
            } else {
                std::cout << printed << " record(s) listed.\n";
            }
            return; // don't run the physical-order loop
        }
        // change color back to save_color
    }

    // Fallback: physical order (unchanged)
    int printed = 0;
    const int32_t start = opt.all ? 1 : a.recno();
    for (int32_t rn = start; rn <= total; ++rn) {
        if (!a.gotoRec(rn)) break;
        if (!a.readCurrent()) continue;

        // default LIST skips deleted unless ALL
        if (!opt.all && a.isDeleted()) continue;

        if (opt.haveFilter && !predicates::eval(a, opt.fld, opt.op, opt.val))
            continue;

        print_row(a, recw);
        ++printed;

        if (!opt.all && opt.limit > 0 && printed >= opt.limit) break;
    }

    if (!opt.all) {
        std::cout << printed << " record(s) listed (limit "
                  << opt.limit << "). Use LIST ALL to show more.\n";
    } else {
        std::cout << printed << " record(s) listed.\n";
    }
}

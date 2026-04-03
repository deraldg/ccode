#include "xbase.hpp"
#include "xbase/dbf_create.hpp"
#include "datatype_index.hpp"
#include "memo/memo_auto.hpp"

// Optional path slot support (SETPATH). If present, resolve relative DBF paths against Slot::DBF.
#if __has_include("cli/cmd_setpath.hpp")
  #include "cli/cmd_setpath.hpp"
  #define HAVE_SETPATH 1
#else
  #define HAVE_SETPATH 0
#endif

#include "cli/order_state.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

namespace dt = dottalk::types;
using Flavor = xbase::dbf_create::Flavor;
using FieldSpec = xbase::dbf_create::FieldSpec;

static inline std::string s8(const std::filesystem::path& p)
{
#if defined(_WIN32)
    auto u = p.u8string();
    return std::string(u.begin(), u.end());
#else
    return p.string();
#endif
}

static inline std::filesystem::path dbf_slot_root_fallback()
{
#if HAVE_SETPATH
    try { return dottalk::paths::get_slot(dottalk::paths::Slot::DBF); } catch (...) {}
#endif
    return std::filesystem::current_path();
}

static inline std::filesystem::path resolve_dbf_slot_path(std::filesystem::path p)
{
    if (p.empty()) return p;
    if (p.is_absolute()) return p;
    return dbf_slot_root_fallback() / p;
}

static std::string trim(std::string s)
{
    auto sp = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    };
    while (!s.empty() && sp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && sp((unsigned char)s.back()))  s.pop_back();
    return s;
}

static std::string up_copy(std::string s)
{
    std::transform(
        s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

static dt::EngineFormat engine_format_for(Flavor flavor) noexcept
{
    switch (flavor)
    {
    case Flavor::MSDOS: return dt::EngineFormat::MSDOS_DBASE;
    case Flavor::FOX26: return dt::EngineFormat::FOX26;
    case Flavor::VFP:   return dt::EngineFormat::VFP;
    case Flavor::X64:   return dt::EngineFormat::VFP; // compatibility-first
    }
    return dt::EngineFormat::Unknown;
}

static std::vector<std::string> split_top(const std::string& inside)
{
    std::vector<std::string> out;
    int depth = 0;
    size_t start = 0;

    for (size_t i = 0; i < inside.size(); ++i) {
        const char c = inside[i];
        if (c == '(') ++depth;
        else if (c == ')' && depth > 0) --depth;
        else if ((c == ',' || c == ';') && depth == 0) {
            out.emplace_back(trim(inside.substr(start, i - start)));
            start = i + 1;
        }
    }

    if (start < inside.size()) out.emplace_back(trim(inside.substr(start)));
    return out;
}

static bool parse_one(const std::string& part,
                      FieldSpec& fs,
                      dt::EngineFormat fmt,
                      Flavor flavor,
                      std::string& err)
{
    std::istringstream ps(part);
    std::string fname, typeAndParams;
    if (!(ps >> fname >> typeAndParams)) {
        err = "Invalid field clause: '" + part + "'";
        return false;
    }
    if (fname.empty() || typeAndParams.empty()) {
        err = "Invalid field clause: '" + part + "'";
        return false;
    }

    fs.name = fname;
    const char T = (char)std::toupper((unsigned char)typeAndParams[0]);

    const dt::ValidationResult vr = dt::validate_type_for_format(T, fmt);
    if (!vr.ok) {
        err = "CREATE: field type '" + std::string(1, T) +
              "' is not supported for this format.";
        return false;
    }

    if (!xbase::dbf_create::supports_type_now(T, flavor)) {
        err = "CREATE: field type '" + std::string(1, T) +
              "' is not implemented for this CREATE flavor.";
        return false;
    }

    std::string params;
    const auto lp = typeAndParams.find('(');
    if (lp != std::string::npos) {
        const auto rp = typeAndParams.find_last_of(')');
        if (rp == std::string::npos || rp < lp) {
            err = "Malformed type parameter list in '" + part + "'";
            return false;
        }
        params = typeAndParams.substr(lp + 1, rp - lp - 1);
    }

    fs.type = T;
    fs.dec  = 0;
    fs.len  = 0;

    try
    {
        if (T == 'C') {
            if (params.empty()) {
                err = "CREATE: C type requires a length, e.g. C(20).";
                return false;
            }
            fs.len = (std::uint8_t)std::stoi(params);
            if (fs.len == 0) {
                err = "CREATE: C type length must be greater than zero.";
                return false;
            }
        }
        else if (T == 'N' || T == 'F') {
            if (params.empty()) {
                err = "CREATE: numeric/float type requires length, e.g. N(10,2).";
                return false;
            }
            const auto comma = params.find(',');
            if (comma == std::string::npos) {
                fs.len = (std::uint8_t)std::stoi(params);
                fs.dec = 0;
            } else {
                fs.len = (std::uint8_t)std::stoi(params.substr(0, comma));
                fs.dec = (std::uint8_t)std::stoi(params.substr(comma + 1));
            }
            if (fs.len == 0) {
                err = "CREATE: numeric/float length must be greater than zero.";
                return false;
            }
            if (fs.dec > fs.len) {
                err = "CREATE: decimal count cannot exceed field length.";
                return false;
            }
        }
        else if (T == 'D') {
            fs.len = 8;
            fs.dec = 0;
        }
        else if (T == 'L') {
            fs.len = 1;
            fs.dec = 0;
        }
        else if (T == 'M') {
            if (flavor == Flavor::X64) {
                fs.len = 8;
            } else {
                fs.len = 10;
            }
            fs.dec = 0;
        }
        else if (T == 'I') {
            fs.len = 4;
            fs.dec = 0;
        }
        else if (T == 'B') {
            fs.len = 8;
            fs.dec = 0;
        }
        else if (T == 'Y') {
            fs.len = 8;
            fs.dec = 4;
        }
        else if (T == 'T') {
            fs.len = 8;
            fs.dec = 0;
        }
        else {
            err = "CREATE: unsupported field type parser path for '" + std::string(1, T) + "'";
            return false;
        }
    }
    catch (const std::exception&)
    {
        err = "CREATE: invalid type parameter list in '" + part + "'";
        return false;
    }

    return true;
}

static bool parse_field_list(std::istringstream& args,
                             std::vector<FieldSpec>& out,
                             std::string& tableName,
                             Flavor& flavor,
                             std::string& err)
{
    flavor = Flavor::MSDOS;

    std::string first;
    if (!(args >> first)) {
        err = "CREATE: missing table name.";
        return false;
    }

    const std::string firstUp = up_copy(first);

    if (firstUp == "MSDOS" || firstUp == "DBASE") {
        flavor = Flavor::MSDOS;
        if (!(args >> tableName)) {
            err = "CREATE: missing table name after MSDOS/DBASE.";
            return false;
        }
    }
    else if (firstUp == "FOX26" || firstUp == "FOXPRO26" || firstUp == "FOXPRO") {
        flavor = Flavor::FOX26;
        if (!(args >> tableName)) {
            err = "CREATE: missing table name after FOX26/FOXPRO.";
            return false;
        }
    }
    else if (firstUp == "VFP") {
        flavor = Flavor::VFP;
        if (!(args >> tableName)) {
            err = "CREATE: missing table name after VFP.";
            return false;
        }
    }
    else if (firstUp == "X64") {
        flavor = Flavor::X64;
        if (!(args >> tableName)) {
            err = "CREATE: missing table name after X64.";
            return false;
        }
    }
    else {
        tableName = first;
    }

    char ch;
    if (!(args >> ch) || ch != '(') {
        err = "CREATE: expected '(' after table name.";
        return false;
    }

    std::string inside;
    int depth = 1;
    char c;

    while (args.get(c))
    {
        if (c == '(') ++depth;
        else if (c == ')')
        {
            if (--depth == 0) break;
        }
        inside.push_back(c);
    }
    if (depth != 0) {
        err = "CREATE: unmatched parentheses in field list.";
        return false;
    }

    auto parts = split_top(inside);
    out.clear();

    const dt::EngineFormat fmt = engine_format_for(flavor);

    for (const auto& p : parts)
    {
        if (p.empty()) continue;
        FieldSpec fs;
        if (!parse_one(p, fs, fmt, flavor, err)) return false;
        out.push_back(fs);
    }

    if (out.empty()) {
        err = "CREATE: no fields specified.";
        return false;
    }

    return true;
}

} // namespace

void cmd_CREATE(xbase::DbArea& area, std::istringstream& args)
{
    const std::string raw_args = args.str();

    std::vector<FieldSpec> fields;
    std::string table;
    Flavor flavor = Flavor::MSDOS;
    std::string err;

    std::cout << "[CREATE RAW] " << raw_args << "\n";

    if (!parse_field_list(args, fields, table, flavor, err))
    {
        if (!err.empty()) {
            std::cout << err << "\n";
        }
        std::cout << "CREATE <name> (<FIELD TYPE(len[,dec]) ...>)\n"
                  << "CREATE MSDOS <name> (...)\n"
                  << "CREATE FOX26 <name> (...)\n"
                  << "CREATE VFP <name> (...)\n"
                  << "CREATE X64 <name> (...)\n"
                  << "XBASE currently implemented CREATE types: C(n), N(n[,d]), F(n[,d]), D, L, M\n"
                  << "VFP and x64 currently implemented CREATE types: C(n), N(n[,d]), F(n[,d]), D, L, M, I, B, Y, T\n";
        return;
    }

    std::cout << "[CREATE DEBUG] parsed fields: " << fields.size() << "\n";
    for (const auto& f : fields)
    {
        std::cout << "  " << f.name << " "
                  << f.type << "("
                  << int(f.len) << ","
                  << int(f.dec) << ")\n";
    }

    if (area.isOpen())
    {
        orderstate::clearOrder(area);
        area.close();
    }

    const std::filesystem::path outp =
        resolve_dbf_slot_path(std::filesystem::path(xbase::dbNameWithExt(table)));
    const std::string path = s8(outp);

    if (!xbase::dbf_create::create_dbf(path, fields, flavor, err))
    {
        std::cout << "CREATE failed: " << err << "\n";
        return;
    }

    try
    {
        area.open(path);
    }
    catch (const std::exception& ex)
    {
        std::cout << "CREATE failed: file written but could not reopen table: "
                  << ex.what() << "\n";
        return;
    }
    catch (...)
    {
        std::cout << "CREATE failed: file written but could not reopen table.\n";
        return;
    }

    const bool hasMemo = std::any_of(
        fields.begin(), fields.end(),
        [](const FieldSpec& f) { return f.type == 'M' || f.type == 'm'; });

    if (hasMemo) {
        const std::string openedPath = area.filename().empty() ? path : area.filename();
        std::string memo_err;
        if (!cli_memo::memo_auto_on_use(area, openedPath, true, memo_err)) {
            std::cout << "Memo attach failed: " << memo_err << "\n";
        }
    }

    std::cout << "Created " << path
              << " [" << xbase::dbf_create::flavor_name(flavor) << "]"
              << (hasMemo ? " (+ memo)" : "") << "\n";
    std::cout << "Opened " << area.filename()
              << " with " << area.recCount() << " records.\n";
}
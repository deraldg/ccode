#include "xbase/fields.hpp"
#include "xbase/dbf_create.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace fields {
namespace {

std::string trim_copy(std::string s)
{
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };

    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

std::string upper_copy(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return s;
}

bool is_name_char(unsigned char ch)
{
    return std::isalnum(ch) || ch == '_';
}

Result not_implemented(const std::string& msg)
{
    Result r;
    r.status = Status::NotImplemented;
    r.message = msg;
    return r;
}

bool parse_uint8_strict(const std::string& s, std::uint8_t& out)
{
    const std::string t = trim_copy(s);
    if (t.empty()) return false;

    unsigned long v = 0;
    try {
        std::size_t pos = 0;
        v = std::stoul(t, &pos, 10);
        if (pos != t.size()) return false;
    } catch (...) {
        return false;
    }

    if (v > 255UL) return false;
    out = static_cast<std::uint8_t>(v);
    return true;
}

bool default_fixed_length_for_type(char t, std::uint8_t& len, std::uint8_t& dec)
{
    switch (static_cast<char>(std::toupper(static_cast<unsigned char>(t)))) {
    case 'D':
        len = 8;
        dec = 0;
        return true;
    case 'L':
        len = 1;
        dec = 0;
        return true;
    default:
        return false;
    }
}

bool is_supported_append_type(char t) noexcept
{
    switch (static_cast<char>(std::toupper(static_cast<unsigned char>(t)))) {
    case 'C':
    case 'N':
    case 'D':
    case 'L':
        return true;
    default:
        return false;
    }
}

xbase::dbf_create::FieldSpec to_field_spec(const xbase::FieldDef& fd)
{
    xbase::dbf_create::FieldSpec s;
    s.name = fd.name;
    s.type = fd.type;
    s.len  = fd.length;
    s.dec  = fd.decimals;
    return s;
}

xbase::dbf_create::Flavor flavor_from_db(const xbase::DbArea& db)
{
    switch (db.versionByte()) {
    case 0x30:
    case 0x31:
    case 0x32:
        return xbase::dbf_create::Flavor::VFP;
    case 0xF5:
        return xbase::dbf_create::Flavor::FOX26;
    case 0x64:
        return xbase::dbf_create::Flavor::X64;
    default:
        return xbase::dbf_create::Flavor::MSDOS;
    }
}

std::string default_field_value(const xbase::FieldDef& fd)
{
    const char t = static_cast<char>(std::toupper(static_cast<unsigned char>(fd.type)));
    switch (t) {
    case 'C':
    case 'N':
        return std::string(fd.length, ' ');
    case 'D':
        return std::string(8, ' ');
    case 'L':
        return "?";
    default:
        return std::string(fd.length, ' ');
    }
}

std::filesystem::path make_temp_dbf_path(const xbase::DbArea& db)
{
    const std::filesystem::path finalPath(db.filename());
    return finalPath.parent_path() /
        (finalPath.stem().string() + ".__fldtmp" + finalPath.extension().string());
}

std::filesystem::path make_backup_dbf_path(const xbase::DbArea& db)
{
    const std::filesystem::path finalPath(db.filename());
    return finalPath.parent_path() /
        (finalPath.stem().string() + ".__fldbak" + finalPath.extension().string());
}

Result append_rewrite_table(xbase::DbArea& db,
                            const xbase::FieldDef& fd,
                            const AppendOptions& opts)
{
    Result r;
    (void)opts;

    const std::string finalPathStr = db.filename();
    const std::filesystem::path tempPath = make_temp_dbf_path(db);
    const std::filesystem::path backupPath = make_backup_dbf_path(db);

    std::vector<xbase::dbf_create::FieldSpec> outFields;
    outFields.reserve(static_cast<std::size_t>(db.fieldCount() + 1));
    for (const auto& f : db.fields()) {
        outFields.push_back(to_field_spec(f));
    }
    outFields.push_back(to_field_spec(fd));

    std::string err;
    const auto flavor = flavor_from_db(db);
    if (!xbase::dbf_create::create_dbf(tempPath.string(), outFields, flavor, err)) {
        r.status = Status::Failed;
        r.message = "FIELDMGR APPEND: create temp table failed: " + err;
        return r;
    }

    try {
        xbase::DbArea out;
        out.open(tempPath.string());

        for (int rec = 1; rec <= db.recCount(); ++rec) {
            if (!db.gotoRec(rec)) {
                r.status = Status::Failed;
                r.message = "FIELDMGR APPEND: failed reading source record";
                out.close();
                std::error_code ec;
                std::filesystem::remove(tempPath, ec);
                return r;
            }

            if (!out.appendBlank()) {
                r.status = Status::Failed;
                r.message = "FIELDMGR APPEND: failed appending destination record";
                out.close();
                std::error_code ec;
                std::filesystem::remove(tempPath, ec);
                return r;
            }

            for (int i = 0; i < db.fieldCount(); ++i) {
                if (!out.set(i + 1, db.get(i + 1))) {
                    r.status = Status::Failed;
                    r.message = "FIELDMGR APPEND: failed copying field data";
                    out.close();
                    std::error_code ec;
                    std::filesystem::remove(tempPath, ec);
                    return r;
                }
            }

            if (!out.set(out.fieldCount(), default_field_value(fd))) {
                r.status = Status::Failed;
                r.message = "FIELDMGR APPEND: failed default-filling appended field";
                out.close();
                std::error_code ec;
                std::filesystem::remove(tempPath, ec);
                return r;
            }

            if (db.isDeleted()) {
                if (!out.deleteCurrent()) {
                    r.status = Status::Failed;
                    r.message = "FIELDMGR APPEND: failed preserving deleted flag";
                    out.close();
                    std::error_code ec;
                    std::filesystem::remove(tempPath, ec);
                    return r;
                }
            }
        }

        out.close();
    } catch (const std::exception& ex) {
        r.status = Status::Failed;
        r.message = std::string("FIELDMGR APPEND: temp table copy failed: ") + ex.what();
        std::error_code ec;
        std::filesystem::remove(tempPath, ec);
        return r;
    }

    db.close();

    std::error_code ec;
    std::filesystem::remove(backupPath, ec);
    ec.clear();

    std::filesystem::rename(finalPathStr, backupPath, ec);
    if (ec) {
        r.status = Status::Failed;
        r.message = "FIELDMGR APPEND: failed to rename original to backup";
        std::filesystem::remove(tempPath, ec);
        return r;
    }

    ec.clear();
    std::filesystem::rename(tempPath, finalPathStr, ec);
    if (ec) {
        std::error_code rc;
        std::filesystem::rename(backupPath, finalPathStr, rc);
        r.status = Status::Failed;
        r.message = "FIELDMGR APPEND: failed to swap temp table into place";
        return r;
    }

    try {
        db.open(finalPathStr);
    } catch (const std::exception& ex) {
        r.status = Status::Failed;
        r.message = std::string("FIELDMGR APPEND: rewrite completed, but reopen failed: ") + ex.what();
        return r;
    }

    r.status = Status::Ok;
    r.changed = true;
    r.message = "FIELDMGR APPEND: field appended successfully";
    return r;
}

} // anonymous namespace

std::string usage()
{
    return
        "FIELDMGR\n"
        "FIELDMGR SHOW\n"
        "FIELDMGR LIST\n"
        "FIELDMGR APPEND <name> <type>(<len>[,<dec>])\n"
        "FIELDMGR DELETE <name>\n"
        "FIELDMGR MODIFY <name> NAME <newname>\n"
        "FIELDMGR MODIFY <name> TYPE <type>(<len>[,<dec>])\n"
        "FIELDMGR MODIFY <name> TO <newname> <type>(<len>[,<dec>])\n"
        "FIELDMGR COPY TO <target>\n"
        "FIELDMGR COPY TO <target> MAP ...\n"
        "FIELDMGR VALIDATE\n"
        "FIELDMGR CHECK\n"
        "FIELDMGR REBUILD INDEXES\n"
        "\n"
        "Notes:\n"
        "  - FIELDMGR changes table structure.\n"
        "  - APPEND adds fields only at the end.\n"
        "  - Rearranging fields is not supported.\n"
        "  - Structural changes preserve existing records; they do not PACK the table.\n";
}

std::string opName(Op op)
{
    switch (op) {
    case Op::None:           return "NONE";
    case Op::Show:           return "SHOW";
    case Op::Append:         return "APPEND";
    case Op::DeleteField:    return "DELETE";
    case Op::ModifyName:     return "MODIFY NAME";
    case Op::ModifyType:     return "MODIFY TYPE";
    case Op::ModifyTo:       return "MODIFY TO";
    case Op::CopyTo:         return "COPY TO";
    case Op::CopyToMap:      return "COPY TO MAP";
    case Op::Validate:       return "VALIDATE";
    case Op::Check:          return "CHECK";
    case Op::RebuildIndexes: return "REBUILD INDEXES";
    default:                 return "UNKNOWN";
    }
}

bool validateFieldName(const std::string& nameIn, std::string& err)
{
    const std::string name = trim_copy(nameIn);

    if (name.empty()) {
        err = "field name is empty";
        return false;
    }

    if (name.size() > 10) {
        err = "field name exceeds 10 characters";
        return false;
    }

    const unsigned char first = static_cast<unsigned char>(name.front());
    if (!(std::isalpha(first) || first == '_')) {
        err = "field name must start with a letter or underscore";
        return false;
    }

    for (unsigned char ch : name) {
        if (!is_name_char(ch)) {
            err = "field name contains invalid characters";
            return false;
        }
    }

    return true;
}

bool validateFieldDef(const xbase::FieldDef& fd, std::string& err)
{
    if (!validateFieldName(fd.name, err)) {
        return false;
    }

    const char t = static_cast<char>(std::toupper(static_cast<unsigned char>(fd.type)));

    if (!is_supported_append_type(t)) {
        err = std::string("unsupported field type '") + t + "'";
        return false;
    }

    switch (t) {
    case 'C':
        if (fd.length == 0) {
            err = "character field length must be > 0";
            return false;
        }
        if (fd.decimals != 0) {
            err = "character field decimals must be 0";
            return false;
        }
        return true;

    case 'N':
        if (fd.length == 0) {
            err = "numeric field length must be > 0";
            return false;
        }
        if (fd.decimals > fd.length) {
            err = "numeric field decimals cannot exceed length";
            return false;
        }
        return true;

    case 'D':
        if (fd.length != 8) {
            err = "date field length must be 8";
            return false;
        }
        if (fd.decimals != 0) {
            err = "date field decimals must be 0";
            return false;
        }
        return true;

    case 'L':
        if (fd.length != 1) {
            err = "logical field length must be 1";
            return false;
        }
        if (fd.decimals != 0) {
            err = "logical field decimals must be 0";
            return false;
        }
        return true;

    default:
        err = "unsupported field type";
        return false;
    }
}

bool parseFieldSpec(const std::string& textIn, xbase::FieldDef& out, std::string& err)
{
    out = {};

    std::string text = trim_copy(textIn);
    if (text.empty()) {
        err = "empty field specification";
        return false;
    }

    std::istringstream iss(text);
    std::string name;
    std::string spec;

    if (!(iss >> name)) {
        err = "missing field name";
        return false;
    }

    if (!(iss >> spec)) {
        err = "missing field type specification";
        return false;
    }

    std::string extra;
    if (iss >> extra) {
        err = "too many tokens in field specification";
        return false;
    }

    name = upper_copy(trim_copy(name));
    if (!validateFieldName(name, err)) {
        return false;
    }

    spec = upper_copy(trim_copy(spec));
    if (spec.empty()) {
        err = "empty type specification";
        return false;
    }

    const char type = spec.front();
    std::uint8_t len = 0;
    std::uint8_t dec = 0;

    const std::size_t lpar = spec.find('(');
    const std::size_t rpar = spec.find(')');

    if (lpar == std::string::npos && rpar == std::string::npos) {
        if (!default_fixed_length_for_type(type, len, dec)) {
            err = "type requires explicit length";
            return false;
        }
    } else {
        if (lpar == std::string::npos || rpar == std::string::npos || rpar <= lpar + 1) {
            err = "malformed type specification";
            return false;
        }

        if (lpar != 1) {
            err = "type specification must begin with single-letter type";
            return false;
        }

        const std::string inside = spec.substr(lpar + 1, rpar - lpar - 1);
        const std::size_t comma = inside.find(',');

        if (comma == std::string::npos) {
            if (!parse_uint8_strict(inside, len)) {
                err = "invalid field length";
                return false;
            }
            dec = 0;
        } else {
            const std::string lhs = inside.substr(0, comma);
            const std::string rhs = inside.substr(comma + 1);

            if (!parse_uint8_strict(lhs, len)) {
                err = "invalid field length";
                return false;
            }
            if (!parse_uint8_strict(rhs, dec)) {
                err = "invalid decimal count";
                return false;
            }
        }

        if (rpar != spec.size() - 1) {
            err = "unexpected trailing characters in type specification";
            return false;
        }
    }

    out.name = name;
    out.type = type;
    out.length = len;
    out.decimals = dec;

    return validateFieldDef(out, err);
}

int findFieldCI(const xbase::DbArea& db, const std::string& nameIn)
{
    const std::string want = upper_copy(trim_copy(nameIn));
    const auto& f = db.fields();

    for (int i = 0; i < static_cast<int>(f.size()); ++i) {
        if (upper_copy(trim_copy(f[i].name)) == want) {
            return i;
        }
    }
    return -1;
}

bool hasFieldCI(const xbase::DbArea& db, const std::string& name)
{
    return findFieldCI(db, name) >= 0;
}

FieldProtectionInfo getFieldProtectionInfo(const xbase::DbArea& db,
                                           const std::string& fieldName)
{
    (void)db;
    (void)fieldName;

    FieldProtectionInfo info;
    return info;
}

Result show(const xbase::DbArea& db)
{
    Result r;

    if (!db.isOpen()) {
        r.status = Status::InvalidState;
        r.message = "FIELDMGR: no table open";
        return r;
    }

    std::ostringstream oss;
    oss << "No  "
        << std::left << std::setw(12) << "Name"
        << std::setw(6) << "Type"
        << std::right << std::setw(5) << "Len"
        << std::setw(5) << "Dec"
        << "\n";

    oss << "--  "
        << std::left << std::setw(12) << "------------"
        << std::setw(6) << "----"
        << std::right << std::setw(5) << "---"
        << std::setw(5) << "---"
        << "\n";

    const auto& f = db.fields();
    for (int i = 0; i < static_cast<int>(f.size()); ++i) {
        oss << std::right << std::setw(2) << (i + 1) << "  "
            << std::left  << std::setw(12) << f[i].name
            << std::setw(6) << std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(f[i].type))))
            << std::right << std::setw(5) << static_cast<int>(f[i].length)
            << std::setw(5) << static_cast<int>(f[i].decimals)
            << "\n";
    }

    r.status = Status::Ok;
    r.message = oss.str();
    return r;
}

IndexImpact assessAppendIndexImpact(const xbase::DbArea& db,
                                    const xbase::FieldDef& fd)
{
    (void)fd;

    const auto* idx = db.indexManagerPtr();
    if (!idx) {
        return IndexImpact::None;
    }

    return IndexImpact::RebuildRecommended;
}

Result append(xbase::DbArea& db,
              const xbase::FieldDef& fdIn,
              const AppendOptions& opts)
{
    Result r;

    if (!db.isOpen()) {
        r.status = Status::InvalidState;
        r.message = "FIELDMGR APPEND: no table open";
        return r;
    }

    if (db.memoKind() != xbase::DbArea::MemoKind::NONE) {
        r.status = Status::Unsupported;
        r.message = "FIELDMGR APPEND: memo tables not supported yet";
        return r;
    }

    xbase::FieldDef fd = fdIn;
    fd.name = upper_copy(trim_copy(fd.name));
    fd.type = static_cast<char>(std::toupper(static_cast<unsigned char>(fd.type)));

    std::string err;
    if (!validateFieldDef(fd, err)) {
        r.status = Status::InvalidArgument;
        r.message = "FIELDMGR APPEND: " + err;
        return r;
    }

    if (hasFieldCI(db, fd.name)) {
        r.status = Status::InvalidArgument;
        r.message = "FIELDMGR APPEND: duplicate field name '" + fd.name + "'";
        return r;
    }

    if (db.fieldCount() >= xbase::MAX_FIELDS) {
        r.status = Status::InvalidArgument;
        r.message = "FIELDMGR APPEND: maximum field count reached";
        return r;
    }

    r.indexImpact = assessAppendIndexImpact(db, fd);
    r.rebuildSuggested =
        (r.indexImpact == IndexImpact::RebuildRecommended ||
         r.indexImpact == IndexImpact::RebuildRequired);

    if (opts.failIfIndexesPresent &&
        (r.indexImpact == IndexImpact::RebuildRecommended ||
         r.indexImpact == IndexImpact::RebuildRequired ||
         r.indexImpact == IndexImpact::Blocked)) {
        r.status = Status::InvalidState;
        r.message = "FIELDMGR APPEND: indexes present; structural rewrite refused by option";
        return r;
    }

    r = append_rewrite_table(db, fd, opts);
    if (r.status == Status::Ok && r.rebuildSuggested) {
        r.message += " [index rebuild recommended]";
    }
    return r;
}

Result deleteField(xbase::DbArea& db, const std::string& fieldName)
{
    (void)db;
    (void)fieldName;
    return not_implemented("FIELDMGR DELETE: not implemented yet");
}

Result modifyName(xbase::DbArea& db,
                  const std::string& oldName,
                  const std::string& newName)
{
    (void)db;
    (void)oldName;
    (void)newName;
    return not_implemented("FIELDMGR MODIFY NAME: not implemented yet");
}

Result modifyType(xbase::DbArea& db,
                  const std::string& fieldName,
                  const xbase::FieldDef& newDef)
{
    (void)db;
    (void)fieldName;
    (void)newDef;
    return not_implemented("FIELDMGR MODIFY TYPE: not implemented yet");
}

Result modifyTo(xbase::DbArea& db,
                const std::string& oldName,
                const xbase::FieldDef& newDef)
{
    (void)db;
    (void)oldName;
    (void)newDef;
    return not_implemented("FIELDMGR MODIFY TO: not implemented yet");
}

Result copyTo(xbase::DbArea& db, const std::string& targetPath)
{
    (void)db;
    (void)targetPath;
    return not_implemented("FIELDMGR COPY TO: not implemented yet");
}

Result copyToMap(xbase::DbArea& db,
                 const std::string& targetPath,
                 const CopyPlan& plan)
{
    (void)db;
    (void)targetPath;
    (void)plan;
    return not_implemented("FIELDMGR COPY TO MAP: not implemented yet");
}

Result validate(const xbase::DbArea& db)
{
    (void)db;
    return not_implemented("FIELDMGR VALIDATE: not implemented yet");
}

Result check(const xbase::DbArea& db)
{
    (void)db;
    return not_implemented("FIELDMGR CHECK: not implemented yet");
}

Result rebuildIndexes(xbase::DbArea& db)
{
    (void)db;
    return not_implemented("FIELDMGR REBUILD INDEXES: not implemented yet");
}

} // namespace fields

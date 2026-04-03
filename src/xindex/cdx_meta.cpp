#include "cdx/cdx_meta.hpp"
#include "xbase.hpp"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

namespace xindex::cdxmeta {

static std::string sidecar_path_(const std::string& cdx) {
    return cdx + ".meta";
}

static std::string kind_to_string_(xbase::AreaKind k) {
    switch (k) {
        case xbase::AreaKind::V32:  return "v32";
        case xbase::AreaKind::V64:  return "v64";
        case xbase::AreaKind::V128: return "v128";
        case xbase::AreaKind::Tup:  return "tup";
        case xbase::AreaKind::Unknown:
        default:                    return "unknown";
    }
}

static std::string upper_ascii_copy_(std::string s) {
    for (char& ch : s) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return s;
}

// Stable 64-bit FNV-1a hash.
// Do NOT use std::hash here; it is not stable across runs/builds/platforms.
static std::uint64_t fnv1a64_(const std::string& s) {
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s) {
        h ^= static_cast<std::uint64_t>(c);
        h *= 1099511628211ULL;
    }
    return h;
}

static std::uint64_t hash_schema_(const xbase::DbArea& a) {
    std::string s;
    const auto& defs = a.fields();

    for (const auto& f : defs) {
        s += upper_ascii_copy_(f.name);
        s.push_back('|');
        s.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(f.type))));
        s.push_back('|');
        s += std::to_string(static_cast<unsigned>(f.length));
        s.push_back('|');
        s += std::to_string(static_cast<unsigned>(f.decimals));
        s.push_back(';');
    }

    return fnv1a64_(s);
}

TableIdentity build_identity(const xbase::DbArea& a) {
    TableIdentity t;
    t.kind        = kind_to_string_(a.kind());
    t.version     = a.versionByte();
    t.rec_len     = static_cast<std::uint32_t>(a.recLength());
    t.field_count = static_cast<std::uint32_t>(a.fieldCount());
    t.schema_hash = hash_schema_(a);
    t.source      = a.filename();
    return t;
}

std::optional<MetaRecord> read_meta(const std::string& cdx_path, std::string* err) {
    if (err) err->clear();

    std::ifstream in(sidecar_path_(cdx_path), std::ios::binary);
    if (!in) {
        return std::nullopt;
    }

    MetaRecord m{};
    std::string line;

    try {
        while (std::getline(in, line)) {
            const auto pos = line.find('=');
            if (pos == std::string::npos) continue;

            const std::string k = line.substr(0, pos);
            const std::string v = line.substr(pos + 1);

            if (k == "META_VERSION")      m.meta_version       = static_cast<std::uint32_t>(std::stoul(v));
            else if (k == "BACKEND")      m.backend            = v;
            else if (k == "KIND")         m.table.kind         = v;
            else if (k == "VERSION")      m.table.version      = static_cast<std::uint8_t>(std::stoul(v));
            else if (k == "RECLEN")       m.table.rec_len      = static_cast<std::uint32_t>(std::stoul(v));
            else if (k == "FIELDS")       m.table.field_count  = static_cast<std::uint32_t>(std::stoul(v));
            else if (k == "HASH")         m.table.schema_hash  = static_cast<std::uint64_t>(std::stoull(v));
            else if (k == "SOURCE")       m.table.source       = v;
        }
    } catch (const std::exception& ex) {
        if (err) *err = std::string("read_meta parse error: ") + ex.what();
        return std::nullopt;
    }

    return m;
}

bool write_meta(const std::string& cdx_path, const MetaRecord& m, std::string* err) {
    if (err) err->clear();

    std::ofstream out(sidecar_path_(cdx_path), std::ios::binary | std::ios::trunc);
    if (!out) {
        if (err) *err = "write_meta: cannot open sidecar";
        return false;
    }

    out << "META_VERSION=" << m.meta_version << "\n";
    out << "BACKEND="      << m.backend << "\n";
    out << "KIND="         << m.table.kind << "\n";
    out << "VERSION="      << static_cast<unsigned>(m.table.version) << "\n";
    out << "RECLEN="       << m.table.rec_len << "\n";
    out << "FIELDS="       << m.table.field_count << "\n";
    out << "HASH="         << m.table.schema_hash << "\n";
    out << "SOURCE="       << m.table.source << "\n";

    if (!out) {
        if (err) *err = "write_meta: write failed";
        return false;
    }

    return true;
}

bool matches(const TableIdentity& a, const MetaRecord& b) {
    return a.kind        == b.table.kind &&
           a.version     == b.table.version &&
           a.rec_len     == b.table.rec_len &&
           a.field_count == b.table.field_count &&
           a.schema_hash == b.table.schema_hash;
}

} // namespace xindex::cdxmeta
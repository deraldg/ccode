// src/cli/cmd_copy.cpp
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <system_error>

#include "xbase.hpp"
#include "xbase/dbf_create.hpp"
#include "textio.hpp"

using namespace xbase;
using Flavor = xbase::dbf_create::Flavor;
using FieldSpec = xbase::dbf_create::FieldSpec;

namespace {

static inline std::string up(std::string s)   { return textio::up(std::move(s)); }
static inline std::string trim(std::string s) { return textio::trim(std::move(s)); }

static bool token_is(const std::string& t, const char* u) { return up(t) == u; }

static void usage_copy() {
    std::cout
        << "Usage:\n"
        << "  COPY TO <DBFNAME> [WITH SIDECARS] [OVERWRITE]\n"
        << "  COPY TO <DBFNAME> AS <MSDOS|DBASE|FOX26|FOXPRO|VFP|X64> [OVERWRITE]\n"
        << "  COPY FILE <SRC> TO <DST> [OVERWRITE]\n"
        << "\n"
        << "Notes:\n"
        << "  - COPY TO <name>                 : binary copy of the current DBF\n"
        << "  - COPY TO <name> AS <flavor>     : logical table copy/conversion\n"
        << "  - WITH SIDECARS applies to binary COPY TO only\n"
        << "  - x64 output still writes .dbf for now (no .dbfx yet)\n"
        << "\n"
        << "SIDECARS (if present next to the DBF): .inx .cnx .dtx .dti.json\n";
}

static bool copy_file_binary(const std::filesystem::path& src,
                             const std::filesystem::path& dst,
                             bool overwrite,
                             std::string* err = nullptr) {
    std::error_code ec;

    if (!std::filesystem::exists(src, ec) || ec) {
        if (err) *err = "Source does not exist: " + src.string();
        return false;
    }

    if (std::filesystem::exists(dst, ec) && !overwrite) {
        if (err) *err = "Destination exists (use OVERWRITE): " + dst.string();
        return false;
    }

    if (dst.has_parent_path() && !dst.parent_path().empty()) {
        std::filesystem::create_directories(dst.parent_path(), ec);
        ec.clear();
    }

    const auto opt = overwrite ? std::filesystem::copy_options::overwrite_existing
                               : std::filesystem::copy_options::none;

    if (!std::filesystem::copy_file(src, dst, opt, ec) || ec) {
        if (err) *err = "Copy failed: " + src.string() + " -> " + dst.string();
        return false;
    }

    return true;
}

static std::filesystem::path ensure_dbf_path(const std::filesystem::path& p) {
    // Intentionally stays .dbf for all flavors for now.
    return std::filesystem::path(xbase::dbNameWithExt(p.string()));
}

static std::filesystem::path resolve_dst_for_copy_to(const DbArea& a, const std::string& dstToken) {
    // If dstToken includes a directory, honor it; otherwise write next to the open DBF.
    std::filesystem::path dstp(trim(dstToken));
    const std::filesystem::path srcp(a.filename());

    std::filesystem::path leaf = dstp.filename();
    leaf = ensure_dbf_path(leaf);

    const bool has_dir =
        dstp.has_parent_path() &&
        !dstp.parent_path().empty() &&
        dstp.parent_path().string() != ".";

    if (has_dir) return dstp.parent_path() / leaf;
    return srcp.parent_path() / leaf;
}

static bool exists_file(const std::filesystem::path& p) {
    std::error_code ec;
    return std::filesystem::exists(p, ec) && !ec;
}

static void copy_sidecars(const std::filesystem::path& src_dbf,
                          const std::filesystem::path& dst_dbf,
                          bool overwrite) {
    const auto src_dir  = src_dbf.parent_path();
    const auto dst_dir  = dst_dbf.parent_path();
    const auto src_stem = src_dbf.stem().string();
    const auto dst_stem = dst_dbf.stem().string();

    struct SidecarSpec { const char* suffix; };
    const SidecarSpec specs[] = {
        { ".inx" },
        { ".cnx" },
        { ".dtx" },
        { ".dti.json" },
    };

    int found = 0;
    int copied = 0;
    int failed = 0;

    for (const auto& s : specs) {
        const std::filesystem::path src_side = src_dir / (src_stem + s.suffix);
        if (!exists_file(src_side)) continue;
        ++found;

        const std::filesystem::path dst_side = dst_dir / (dst_stem + s.suffix);

        std::string err;
        if (copy_file_binary(src_side, dst_side, overwrite, &err)) {
            ++copied;
        } else {
            ++failed;
            std::cout << "COPY SIDECAR failed: " << err << "\n";
        }
    }

    if (found == 0) {
        std::cout << "COPY SIDECARS: none found.\n";
    } else {
        std::cout << "COPY SIDECARS: found " << found << ", copied " << copied;
        if (failed) std::cout << ", failed " << failed;
        std::cout << ".\n";
    }
}

static bool same_path(const std::filesystem::path& a,
                      const std::filesystem::path& b) {
    std::error_code ec1, ec2;
    const auto ca = std::filesystem::weakly_canonical(a, ec1);
    const auto cb = std::filesystem::weakly_canonical(b, ec2);

#if defined(_WIN32)
    auto sa = (ec1 ? std::filesystem::absolute(a).string() : ca.string());
    auto sb = (ec2 ? std::filesystem::absolute(b).string() : cb.string());
    std::transform(sa.begin(), sa.end(), sa.begin(),
        [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    std::transform(sb.begin(), sb.end(), sb.begin(),
        [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return sa == sb;
#else
    return (ec1 ? std::filesystem::absolute(a) : ca) ==
           (ec2 ? std::filesystem::absolute(b) : cb);
#endif
}

static bool parse_flavor_token(const std::string& tok, Flavor& out) {
    const std::string u = up(trim(tok));

    if (u == "MSDOS" || u == "DBASE") {
        out = Flavor::MSDOS;
        return true;
    }
    if (u == "FOX26" || u == "FOXPRO26" || u == "FOXPRO") {
        out = Flavor::FOX26;
        return true;
    }
    if (u == "VFP") {
        out = Flavor::VFP;
        return true;
    }
    if (u == "X64") {
        out = Flavor::X64;
        return true;
    }
    return false;
}

static std::string flavor_name_local(Flavor f) {
    return xbase::dbf_create::flavor_name(f);
}

static bool build_field_specs_from_area(const DbArea& src,
                                        Flavor flavor,
                                        std::vector<FieldSpec>& out,
                                        std::string& err) {
    out.clear();
    for (const auto& fd : src.fields()) {
        const char T = static_cast<char>(std::toupper(static_cast<unsigned char>(fd.type)));

        if (!xbase::dbf_create::supports_type_now(T, flavor)) {
            err = "COPY TO AS: field type '" + std::string(1, T) +
                  "' is not implemented for target flavor " + flavor_name_local(flavor) + ".";
            return false;
        }

        FieldSpec fs;
        fs.name = fd.name;
        fs.type = T;
        fs.dec  = fd.decimals;
        fs.len  = fd.length;

        // Preserve CREATE semantics for memo width.
        if (T == 'M') {
            fs.len = (flavor == Flavor::X64) ? 8 : 10;
            fs.dec = 0;
        }

        out.push_back(fs);
    }

    if (out.empty()) {
        err = "COPY TO AS: source table has no fields.";
        return false;
    }

    return true;
}

static void remove_conversion_artifacts(const std::filesystem::path& dst_dbf) {
    std::error_code ec;
    const auto dir  = dst_dbf.parent_path();
    const auto stem = dst_dbf.stem().string();

    const std::filesystem::path files[] = {
        dst_dbf,
        dir / (stem + ".inx"),
        dir / (stem + ".cnx"),
        dir / (stem + ".cdx"),
        dir / (stem + ".cdx.meta"),
        dir / (stem + ".dtx"),
        dir / (stem + ".dbt"),
        dir / (stem + ".fpt"),
        dir / (stem + ".dti.json"),
    };

    for (const auto& p : files) {
        std::filesystem::remove(p, ec);
        ec.clear();
    }

    const auto cdx_env = dir / (stem + ".cdx.d");
    std::filesystem::remove_all(cdx_env, ec);
}

static bool logical_copy_to_as(const DbArea& src,
                               const std::filesystem::path& dstp,
                               Flavor flavor,
                               bool overwrite,
                               std::string& err) {
    if (!src.isOpen()) {
        err = "COPY TO AS: no file open.";
        return false;
    }

    const std::filesystem::path srcp(src.filename());
    if (srcp.empty()) {
        err = "COPY TO AS: source filename unavailable.";
        return false;
    }

    if (same_path(srcp, dstp)) {
        err = "COPY TO AS: source and destination are the same file.";
        return false;
    }

    std::error_code ec;
    if (std::filesystem::exists(dstp, ec) && !overwrite) {
        err = "Destination exists (use OVERWRITE): " + dstp.string();
        return false;
    }

    if (dstp.has_parent_path() && !dstp.parent_path().empty()) {
        std::filesystem::create_directories(dstp.parent_path(), ec);
        ec.clear();
    }

    if (overwrite) {
        remove_conversion_artifacts(dstp);
    }

    std::vector<FieldSpec> specs;
    if (!build_field_specs_from_area(src, flavor, specs, err)) {
        return false;
    }

    if (!xbase::dbf_create::create_dbf(dstp.string(), specs, flavor, err)) {
        return false;
    }

    DbArea dst;
    try {
        dst.open(dstp.string());
    } catch (const std::exception& ex) {
        err = "Target created but reopen failed: " + std::string(ex.what());
        return false;
    } catch (...) {
        err = "Target created but reopen failed.";
        return false;
    }

    const int32_t total = src.recCount();

    for (int32_t rn = 1; rn <= total; ++rn) {
        DbArea& s = const_cast<DbArea&>(src); // source navigation/read APIs are non-const
        if (!s.gotoRec(rn) || !s.readCurrent()) {
            err = "COPY TO AS: failed reading source record " + std::to_string(rn);
            return false;
        }

        if (!dst.appendBlank()) {
            err = "COPY TO AS: appendBlank failed at target record " + std::to_string(rn);
            return false;
        }

        for (int i = 1; i <= s.fieldCount(); ++i) {
            if (!dst.set(i, s.get(i))) {
                err = "COPY TO AS: failed setting field " + std::to_string(i) +
                      " at source record " + std::to_string(rn);
                return false;
            }
        }

        if (!dst.writeCurrent()) {
            err = "COPY TO AS: writeCurrent failed at target record " + std::to_string(rn);
            return false;
        }

        if (s.isDeleted()) {
            // Best-effort preserve deleted state.
            if (!dst.deleteCurrent()) {
                err = "COPY TO AS: failed preserving deleted state at record " +
                      std::to_string(rn);
                return false;
            }
        }
    }

    return true;
}

} // namespace

void cmd_COPY(DbArea& a, std::istringstream& iss) {
    std::vector<std::string> tok;
    {
        std::string t;
        while (iss >> t) tok.push_back(t);
    }

    if (tok.empty()) { usage_copy(); return; }

    bool overwrite = false;
    bool with_sidecars = false;

    // Strip OVERWRITE, WITH, SIDECARS tokens (flexibly)
    {
        std::vector<std::string> kept;
        kept.reserve(tok.size());

        for (size_t i = 0; i < tok.size(); ++i) {
            const std::string u = up(tok[i]);

            if (u == "OVERWRITE") { overwrite = true; continue; }

            if (u == "WITH") {
                if (i + 1 < tok.size() && up(tok[i + 1]) == "SIDECARS") {
                    with_sidecars = true;
                    ++i;
                    continue;
                }
                continue;
            }

            if (u == "SIDECARS") { with_sidecars = true; continue; }

            kept.push_back(tok[i]);
        }

        tok.swap(kept);
    }

    if (tok.empty()) { usage_copy(); return; }

    // COPY FILE <src> TO <dst>
    if (token_is(tok[0], "FILE")) {
        if (tok.size() < 4 || !token_is(tok[2], "TO")) { usage_copy(); return; }

        const std::filesystem::path src = trim(tok[1]);
        const std::filesystem::path dst = trim(tok[3]);

        std::string err;
        if (copy_file_binary(src, dst, overwrite, &err)) {
            std::cout << "Copied file to " << dst.string() << "\n";
        } else {
            std::cout << "COPY FILE failed: " << err << "\n";
        }
        return;
    }

    // COPY TO <dbf> [AS <flavor>]
    if (token_is(tok[0], "TO")) {
        if (!a.isOpen()) {
            std::cout << "COPY TO: no file open.\n";
            return;
        }
        if (tok.size() < 2) {
            usage_copy();
            return;
        }

        const std::filesystem::path srcp(a.filename());
        const std::filesystem::path dstp = resolve_dst_for_copy_to(a, tok[1]);

        // Logical conversion branch: COPY TO <name> AS <flavor>
        size_t as_pos = tok.size();
        for (size_t i = 2; i < tok.size(); ++i) {
            if (token_is(tok[i], "AS")) {
                as_pos = i;
                break;
            }
        }

        if (as_pos != tok.size()) {
            if (as_pos + 1 >= tok.size()) {
                usage_copy();
                return;
            }

            Flavor flavor = Flavor::MSDOS;
            if (!parse_flavor_token(tok[as_pos + 1], flavor)) {
                std::cout << "COPY TO AS failed: unknown flavor '" << tok[as_pos + 1] << "'\n";
                return;
            }

            if (with_sidecars) {
                std::cout << "COPY TO AS: WITH SIDECARS ignored for logical conversion.\n";
            }

            std::string err;
            if (!logical_copy_to_as(a, dstp, flavor, overwrite, err)) {
                std::cout << "COPY TO AS failed: " << err << "\n";
                return;
            }

            std::cout << "Copied table to " << dstp.string()
                      << " [" << flavor_name_local(flavor) << "]\n";
            return;
        }

        // Existing binary copy behavior
        std::string err;
        if (!copy_file_binary(srcp, dstp, overwrite, &err)) {
            std::cout << "COPY TO failed: " << err << "\n";
            return;
        }

        std::cout << "Copied DBF to " << dstp.string() << "\n";

        if (with_sidecars) {
            copy_sidecars(srcp, dstp, overwrite);
        }

        return;
    }

    usage_copy();
}
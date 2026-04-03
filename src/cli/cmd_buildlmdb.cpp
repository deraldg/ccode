// src/cli/cmd_buildlmdb.cpp
//
// BUILDLMDB for CDX containers -> LMDB backing store
// One LMDB env per table container; multiple tags as named DBs inside (MDB_MAXDBS=1024).
//
// Public/Backend policy:
// - Public CDX container resolves under INDEXES.
// - LMDB backend env resolves under LMDB.
// - Example:
//     data\indexes\students.cdx
//   -> data\lmdb\students.cdx.d
//
// Workflow policy:
// - BUILDLMDB is shell-safe: no nested std::cin prompt reads.
// - If an existing LMDB env would be destructively rebuilt, caller must supply
//   YES / Y / AUTO / NOPROMPT / QUIET / SILENT explicitly.
// - CLEAN / FORCE archives first and proceeds without the destructive prompt path.

#include "xbase.hpp"
#include "textio.hpp"
#include "cli/path_resolver.hpp"
#include "cli/order_state.hpp"
#include "cdx/cdx.hpp"
#include "xindex/cdx_backend.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>
#include <ctime>
#include <cstdio>
#include <lmdb.h>

constexpr std::uint64_t LMDB_DEFAULT_MAPSIZE =
    128ULL * 1024ULL * 1024ULL; // 128 MiB

namespace fs = std::filesystem;

namespace {

static std::string up_copy(std::string s) {
    for (auto& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

static const char* lmdb_err(int rc) {
    return mdb_strerror(rc);
}

static std::string format_mapsize_bytes(std::uint64_t bytes)
{
    std::ostringstream o;
    const double mib = static_cast<double>(bytes) / (1024.0 * 1024.0);
    o << bytes << " bytes"
      << " (" << std::fixed << std::setprecision((mib < 10.0) ? 1 : 0)
      << mib << " MiB)";
    return o.str();
}

static bool preset_mapsize(const std::string& raw, std::uint64_t& out_bytes)
{
    const std::string u = up_copy(textio::trim(raw));

    if (u == "TINY")   { out_bytes =  32ULL * 1024ULL * 1024ULL; return true; }
    if (u == "SMALL")  { out_bytes =  64ULL * 1024ULL * 1024ULL; return true; }
    if (u == "MEDIUM") { out_bytes = 128ULL * 1024ULL * 1024ULL; return true; }
    if (u == "LARGE")  { out_bytes = 256ULL * 1024ULL * 1024ULL; return true; }
    if (u == "XL")     { out_bytes = 512ULL * 1024ULL * 1024ULL; return true; }
    if (u == "HUGE")   { out_bytes =   1ULL * 1024ULL * 1024ULL * 1024ULL; return true; }

    return false;
}

static bool parse_mapsize_spec(const std::string& raw, std::uint64_t& out_bytes)
{
    std::string s = up_copy(textio::trim(raw));
    if (s.empty()) return false;

    char suffix = '\0';
    const char last = s.back();
    if (last == 'K' || last == 'M' || last == 'G') {
        suffix = last;
        s.pop_back();
        s = textio::trim(s);
    }

    if (s.empty()) return false;

    for (char c : s) {
        if (!std::isdigit((unsigned char)c)) return false;
    }

    std::uint64_t n = 0;
    try {
        n = static_cast<std::uint64_t>(std::stoull(s));
    } catch (...) {
        return false;
    }

    switch (suffix) {
    case 'K': out_bytes = n * 1024ULL; break;
    case 'M': out_bytes = n * 1024ULL * 1024ULL; break;
    case 'G': out_bytes = n * 1024ULL * 1024ULL * 1024ULL; break;
    default:  out_bytes = n; break;
    }

    // Keep a reasonable floor.
    if (out_bytes < (8ULL * 1024ULL * 1024ULL)) return false;

    return true;
}

static void print_buildlmdb_usage()
{
    std::cout
        << "Usage: BUILDLMDB [HELP|?] [MAPSIZE <n[K|M|G]> | SIZE <n[K|M|G]> |\n"
        << "                  TINY|SMALL|MEDIUM|LARGE|XL|HUGE]\n"
        << "                 [YES|AUTO|NOPROMPT] [CLEAN|FORCE] [QUIET]\n"
        << "\n"
        << "  BUILDLMDB\n"
        << "      Rebuild LMDB backing store for the current CDX container.\n"
        << "      Default mapsize is 128 MiB.\n"
        << "\n"
        << "  BUILDLMDB SMALL\n"
        << "      Use preset mapsize 64 MiB.\n"
        << "\n"
        << "  BUILDLMDB MEDIUM\n"
        << "      Use preset mapsize 128 MiB.\n"
        << "\n"
        << "  BUILDLMDB LARGE\n"
        << "      Use preset mapsize 256 MiB.\n"
        << "\n"
        << "  BUILDLMDB MAPSIZE 1G YES\n"
        << "      Rebuild using 1 GiB mapsize for empirical/speed testing.\n"
        << "\n"
        << "  BUILDLMDB CLEAN MAPSIZE 512M YES\n"
        << "      Archive existing LMDB env first, then rebuild at 512 MiB.\n"
        << "\n"
        << "Presets:\n"
        << "  TINY=32 MiB  SMALL=64 MiB  MEDIUM=128 MiB\n"
        << "  LARGE=256 MiB  XL=512 MiB  HUGE=1 GiB\n";
}

static fs::path default_cdx_container_path(xbase::DbArea& area)
{
    std::string stem;
    if (area.isOpen()) {
        stem = area.dbfBasename();
        if (stem.empty()) stem = area.logicalName();
        if (stem.empty()) {
            fs::path n(area.name());
            stem = n.stem().string();
        }
    }
    if (stem.empty()) stem = "table";

    fs::path p = dottalk::paths::resolve_index(stem);
    if (!p.has_extension()) p.replace_extension(".cdx");
    return p;
}

// New architecture:
// Public container (INDEXES) -> backend env (LMDB slot)
static fs::path lmdb_env_dir_for_container(const fs::path& cdx_path)
{
    return dottalk::paths::resolve_lmdb_env_for_cdx(cdx_path);
}

static bool ensure_dir(const fs::path& p)
{
    std::error_code ec;
    if (fs::exists(p, ec)) return fs::is_directory(p, ec);
    return fs::create_directories(p, ec);
}

static std::string timestamp_ymdhms()
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32]{};
    std::snprintf(buf, sizeof(buf),
                  "%04d%02d%02d_%02d%02d%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return std::string(buf);
}

// Move envdir to backups folder next to it
static bool archive_envdir_to_backups(const fs::path& envdir, bool quiet)
{
    std::error_code ec;
    if (!fs::exists(envdir, ec)) return true;
    if (!fs::is_directory(envdir, ec)) {
        std::cout << "BUILDLMDB CLEAN: env path exists but is not a directory: " << envdir.string() << "\n";
        return false;
    }

    fs::path backups = envdir.parent_path() / "backups";
    if (!ensure_dir(backups)) {
        std::cout << "BUILDLMDB CLEAN: unable to create backups dir: " << backups.string() << "\n";
        return false;
    }

    fs::path dst = backups / (envdir.filename().string() + "_" + timestamp_ymdhms());
    fs::rename(envdir, dst, ec);
    if (ec) {
        // fallback copy+remove
        ec.clear();
        fs::copy(envdir, dst, fs::copy_options::recursive, ec);
        if (ec) {
            std::cout << "BUILDLMDB CLEAN: copy failed: " << ec.message() << "\n";
            return false;
        }
        ec.clear();
        fs::remove_all(envdir, ec);
        if (ec) {
            std::cout << "BUILDLMDB CLEAN: remove_all failed after copy: " << ec.message() << "\n";
            return false;
        }
    }

    if (!quiet) {
        std::cout << "BUILDLMDB CLEAN: archived envdir to: " << dst.string() << "\n";
    }
    return true;
}

// Returns true if the directory looks like it contains an active LMDB env
static bool looks_like_existing_lmdb_env(const fs::path& envdir)
{
    std::error_code ec;
    if (!fs::exists(envdir, ec) || !fs::is_directory(envdir, ec)) {
        return false;
    }

    // Strong indicators
    const fs::path data_mdb = envdir / "data.mdb";
    const fs::path lock_mdb = envdir / "lock.mdb";
    if (fs::exists(data_mdb, ec) || fs::exists(lock_mdb, ec)) {
        return true;
    }

    // Fallback: non-empty directory
    return !fs::is_empty(envdir, ec);
}

// Read ASCII-ish keys from main DB (fallback tag list)
static std::vector<std::string> lmdb_read_main_keys(MDB_env* env, size_t max_keys = 2048)
{
    std::vector<std::string> out;
    if (!env) return out;

    MDB_txn* txn = nullptr;
    int rc = mdb_txn_begin(env, nullptr, MDB_RDONLY, &txn);
    if (rc != MDB_SUCCESS || !txn) return out;

    MDB_dbi main_dbi = 0;
    rc = mdb_dbi_open(txn, nullptr, 0, &main_dbi);
    if (rc != MDB_SUCCESS) {
        mdb_txn_abort(txn);
        return out;
    }

    MDB_cursor* cur = nullptr;
    rc = mdb_cursor_open(txn, main_dbi, &cur);
    if (rc != MDB_SUCCESS || !cur) {
        mdb_txn_abort(txn);
        return out;
    }

    MDB_val k{}, v{};
    rc = mdb_cursor_get(cur, &k, &v, MDB_FIRST);
    while (rc == MDB_SUCCESS) {
        if (k.mv_data && k.mv_size > 0 && k.mv_size <= 64) {
            const unsigned char* b = static_cast<const unsigned char*>(k.mv_data);
            bool ok = true;
            for (size_t i = 0; i < k.mv_size; ++i) {
                unsigned char c = b[i];
                if (c < 0x20 || c > 0x7E) { ok = false; break; }
            }
            if (ok) {
                std::string s(reinterpret_cast<const char*>(k.mv_data), k.mv_size);
                s = textio::trim(s);
                s = up_copy(s);
                if (!s.empty()) {
                    bool tok_ok = true;
                    for (char c : s) {
                        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
                            tok_ok = false;
                            break;
                        }
                    }
                    if (tok_ok) out.push_back(s);
                }
            }
        }
        if (out.size() >= max_keys) break;
        rc = mdb_cursor_get(cur, &k, &v, MDB_NEXT);
    }

    mdb_cursor_close(cur);
    mdb_txn_abort(txn);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// Pack recno as 8-byte LE uint64_t
static void pack_recno_le8(uint64_t recno, unsigned char out[8])
{
    out[0] = (unsigned char)(recno & 0xFF);
    out[1] = (unsigned char)((recno >> 8) & 0xFF);
    out[2] = (unsigned char)((recno >> 16) & 0xFF);
    out[3] = (unsigned char)((recno >> 24) & 0xFF);
    out[4] = (unsigned char)((recno >> 32) & 0xFF);
    out[5] = (unsigned char)((recno >> 40) & 0xFF);
    out[6] = (unsigned char)((recno >> 48) & 0xFF);
    out[7] = (unsigned char)((recno >> 56) & 0xFF);
}

// Build one tag from a field name (uppercase, fixed-length + recno suffix)
static bool build_tag_lmdb_from_field(xbase::DbArea& area,
                                      MDB_env* env,
                                      const std::string& tag_name_uc)
{
    if (!area.isOpen()) return false;

    int fld = -1;
    const auto& Fs = area.fields();
    for (int i = 0; i < (int)Fs.size(); ++i) {
        if (textio::ieq(Fs[(size_t)i].name, tag_name_uc)) {
            fld = i + 1;
            break;
        }
    }
    if (fld < 1) return false;

    const auto& fdef = Fs[(size_t)(fld - 1)];
    const int keylen = (int)fdef.length;

    MDB_txn* txn = nullptr;
    int rc0 = mdb_txn_begin(env, nullptr, 0, &txn);
    if (rc0 != MDB_SUCCESS) {
        std::cout << "BUILDLMDB: mdb_txn_begin failed for tag " << tag_name_uc
                  << ": " << rc0 << " (" << lmdb_err(rc0) << ")\n";
        return false;
    }

    MDB_dbi dbi = 0;
    int rc = mdb_dbi_open(txn, tag_name_uc.c_str(), MDB_CREATE, &dbi);
    if (rc != MDB_SUCCESS) {
        std::cout << "BUILDLMDB: dbi_open failed for tag " << tag_name_uc
                  << ": " << rc << " (" << lmdb_err(rc) << ")\n";
        mdb_txn_abort(txn);
        return false;
    }

    // Clear existing contents
    rc = mdb_drop(txn, dbi, 0);
    if (rc != MDB_SUCCESS) {
        std::cout << "BUILDLMDB: drop/clear failed for tag " << tag_name_uc
                  << ": " << rc << " (" << lmdb_err(rc) << ")\n";
        mdb_txn_abort(txn);
        return false;
    }

    const int32_t total = area.recCount();
    std::string k;
    k.reserve((size_t)keylen);
    std::string keybuf;
    keybuf.resize((size_t)keylen + 8);
    unsigned char recbuf[8]{};

    for (int32_t rn = 1; rn <= total; ++rn) {
        if (!area.gotoRec(rn) || !area.readCurrent()) continue;
        if (area.isDeleted()) continue;

        k = area.get(fld);
        if (fdef.type == 'C' || fdef.type == 'c') {
            k = textio::rtrim(k);
            k = textio::up(k);
        }
        if ((int)k.size() > keylen) k.resize((size_t)keylen);
        if ((int)k.size() < keylen) k.append((size_t)(keylen - (int)k.size()), ' ');

        std::memcpy(&keybuf[0], k.data(), (size_t)keylen);
        pack_recno_le8((uint64_t)rn, recbuf);
        std::memcpy(&keybuf[(size_t)keylen], recbuf, 8);

        MDB_val mkey{ keybuf.size(), (void*)keybuf.data() };
        MDB_val mval{ 8, recbuf };

        rc = mdb_put(txn, dbi, &mkey, &mval, 0);
        if (rc != MDB_SUCCESS) {
            std::cout << "BUILDLMDB: put failed for tag " << tag_name_uc
                      << " rec " << rn << ": " << rc << " (" << lmdb_err(rc) << ")\n";
            mdb_txn_abort(txn);
            return false;
        }
    }

    rc = mdb_txn_commit(txn);
    if (rc != MDB_SUCCESS) {
        std::cout << "BUILDLMDB: txn_commit failed for tag " << tag_name_uc
                  << ": " << rc << " (" << lmdb_err(rc) << ")\n";
        return false;
    }

    return true;
}

static bool build_lmdb_env_for_cdx(xbase::DbArea& area,
                                   const fs::path& cdx_container,
                                   std::uint64_t mapsize_bytes,
                                   std::vector<std::string>* out_tags_built)
{
    if (out_tags_built) out_tags_built->clear();

    const fs::path envdir = lmdb_env_dir_for_container(cdx_container);
    if (!ensure_dir(envdir)) {
        std::cout << "BUILDLMDB: unable to create env dir: " << envdir.string() << "\n";
        return false;
    }

    MDB_env* env = nullptr;
    int rc = mdb_env_create(&env);
    if (rc != MDB_SUCCESS || !env) {
        std::cout << "BUILDLMDB: mdb_env_create failed: " << rc << " (" << lmdb_err(rc) << ")\n";
        return false;
    }

    rc = mdb_env_set_maxdbs(env, 1024);
    if (rc != MDB_SUCCESS) {
        std::cout << "BUILDLMDB: mdb_env_set_maxdbs failed: " << rc << " (" << lmdb_err(rc) << ")\n";
        mdb_env_close(env);
        return false;
    }

    rc = mdb_env_set_mapsize(env, mapsize_bytes);
    if (rc != MDB_SUCCESS) {
        std::cout << "BUILDLMDB: mdb_env_set_mapsize failed: "
                  << rc << " (" << lmdb_err(rc) << ")\n";
        mdb_env_close(env);
        return false;
    }

    rc = mdb_env_open(env, envdir.string().c_str(), 0, 0664);
    if (rc != MDB_SUCCESS) {
        std::cout << "BUILDLMDB: mdb_env_open failed: " << rc << " (" << lmdb_err(rc) << ")\n";
        mdb_env_close(env);
        return false;
    }

    // Tag discovery
    std::vector<cdxfile::TagInfo> tags;
    std::error_code ec;
    if (fs::exists(cdx_container, ec) && fs::is_regular_file(cdx_container, ec)) {
        cdxfile::CDXHandle* h = nullptr;
        if (cdxfile::open(cdx_container.string(), h)) {
            (void)cdxfile::read_tagdir(h, tags);
            cdxfile::close(h);
        }
    }

    if (tags.empty()) {
        const std::string active = up_copy(orderstate::activeTag(area));
        if (!active.empty()) {
            tags.push_back(cdxfile::TagInfo{1, active, 0, 0});
        } else {
            const auto keys = lmdb_read_main_keys(env);
            for (const auto& k : keys) {
                tags.push_back(cdxfile::TagInfo{1, k, 0, 0});
            }
        }
    }

    int ok = 0;
    for (const auto& t : tags) {
        const std::string tag_uc = up_copy(t.name);
        if (tag_uc.empty()) continue;
        if (build_tag_lmdb_from_field(area, env, tag_uc)) {
            ++ok;
            if (out_tags_built) out_tags_built->push_back(tag_uc);
        }
    }

    mdb_env_close(env);
    return ok > 0;
}

} // anonymous namespace

// Command entrypoint
void cmd_BUILDLMDB(xbase::DbArea& area, std::istringstream& args)
{
    std::string opt;
    bool do_clean = false;
    bool do_force = false;
    bool auto_confirm = false;
    bool quiet = false;
    bool show_help = false;
    bool mapsize_explicit = false;
    std::uint64_t chosen_mapsize = LMDB_DEFAULT_MAPSIZE;
    std::vector<std::string> unknown_args;
    std::vector<std::string> tokens;

    while (args >> opt) {
        tokens.push_back(up_copy(textio::trim(opt)));
    }

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& t = tokens[i];

        if (t == "HELP" || t == "?" || t == "/?") {
            show_help = true;
        } else if (t == "CLEAN") {
            do_clean = true;
        } else if (t == "FORCE") {
            do_clean = true;
            do_force = true;
        } else if (t == "AUTO" || t == "NOPROMPT" || t == "YES" || t == "Y") {
            auto_confirm = true;
        } else if (t == "QUIET" || t == "SILENT") {
            auto_confirm = true;
            quiet = true;
        } else if (t == "MAPSIZE" || t == "SIZE") {
            if (i + 1 >= tokens.size()) {
                std::cout << "BUILDLMDB: " << t << " requires a value like 64M, 128M, 1G.\n";
                return;
            }
            std::uint64_t parsed = 0;
            if (!parse_mapsize_spec(tokens[++i], parsed)) {
                std::cout << "BUILDLMDB: invalid mapsize: " << tokens[i] << "\n";
                return;
            }
            chosen_mapsize = parsed;
            mapsize_explicit = true;
        } else {
            std::uint64_t preset = 0;
            if (preset_mapsize(t, preset)) {
                chosen_mapsize = preset;
                mapsize_explicit = true;
            } else if (!t.empty()) {
                unknown_args.push_back(t);
            }
        }
    }

    (void)mapsize_explicit; // available if you later want to report preset/default distinction

    if (show_help) {
        print_buildlmdb_usage();
        return;
    }

    if (!unknown_args.empty()) {
        std::cout << "BUILDLMDB: unknown option";
        if (unknown_args.size() > 1) std::cout << "s";
        std::cout << ": ";
        for (size_t i = 0; i < unknown_args.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << unknown_args[i];
        }
        std::cout << "\n";
        print_buildlmdb_usage();
        return;
    }

    if (!area.isOpen()) {
        std::cout << "BUILDLMDB: No table open.\n";
        return;
    }

    fs::path cdx_container;
    if (orderstate::hasOrder(area) && orderstate::isCdx(area)) {
        cdx_container = fs::path(orderstate::orderName(area));
    } else {
        cdx_container = default_cdx_container_path(area);
    }

    const fs::path envdir = lmdb_env_dir_for_container(cdx_container);

    if (!quiet) {
        std::cout << "BUILDLMDB: target container " << cdx_container.string() << "\n";
        std::cout << "BUILDLMDB: LMDB env        " << envdir.string() << "\n";
        std::cout << "BUILDLMDB: mapsize         " << format_mapsize_bytes(chosen_mapsize) << "\n";
    }

    // Safety check: shell-safe confirmation model.
    // Never do a nested std::cin prompt here; require explicit YES/AUTO/NOPROMPT.
    if (!do_clean && !do_force && looks_like_existing_lmdb_env(envdir) && !auto_confirm) {
        std::cout << "\nWARNING: LMDB environment directory already exists and appears to contain data:\n"
                  << "  " << envdir.string() << "\n"
                  << "Rebuilding will DROP and recreate all tag databases inside it.\n"
                  << "This is a destructive operation -- existing index data will be lost.\n\n"
                  << "BUILDLMDB: confirmation required. Re-run with YES, AUTO, or NOPROMPT.\n"
                  << "Examples:\n"
                  << "  BUILDLMDB YES\n"
                  << "  BUILDLMDB CLEAN YES\n"
                  << "  BUILDLMDB MAPSIZE 1G YES\n";
        return;
    }

    if (!do_clean && !do_force && looks_like_existing_lmdb_env(envdir) && auto_confirm && !quiet) {
        std::cout << "BUILDLMDB: auto-confirmed rebuild of existing env.\n";
    }

    // CLEAN/FORCE: archive first
    if (do_clean || do_force) {
        if (!archive_envdir_to_backups(envdir, quiet)) {
            std::cout << "BUILDLMDB: archive failed - aborting.\n";
            return;
        }
    }

    std::vector<std::string> built;
    if (build_lmdb_env_for_cdx(area, cdx_container, chosen_mapsize, &built)) {
        if (!quiet) {
            for (size_t i = 0; i < built.size(); ++i) {
                std::cout << " [" << (i + 1) << "] " << built[i] << " : OK\n";
            }
            std::cout << "BUILDLMDB: done OK=" << built.size() << " tags rebuilt.\n";
            std::cout << "CDX container   : " << cdx_container.string() << "\n";
            std::cout << "LMDB environment: " << envdir.string() << "\n";
            std::cout << "Mapsiz e        : " << format_mapsize_bytes(chosen_mapsize) << "\n";
        }
        return;
    }

    std::cout << "BUILDLMDB: failed to build LMDB environment.\n";
}
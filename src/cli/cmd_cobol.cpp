// src/cli/cmd_cobol.cpp

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "xbase.hpp"
#include "cli/cmd_setpath.hpp"

#include "dt/data/schema.hpp"
#include "dt/data/rowset.hpp"
#include "dt/data/row_codec_dbf.hpp"
#include "dt/data/row_codec_fixed.hpp"
#include "dt/data/fixed_profiles.hpp"

using xbase::DbArea;
namespace fs = std::filesystem;

namespace {

// ----------------------- path helpers ---------------------------------------

static fs::path find_data_root_guess()
{
    fs::path p = fs::current_path();
    for (int i = 0; i < 14; ++i) {
        fs::path cand = p / "data";
        if (fs::exists(cand) && fs::is_directory(cand)) {
            return fs::absolute(cand);
        }
        if (!p.has_parent_path()) break;
        fs::path parent = p.parent_path();
        if (parent == p) break;
        p = parent;
    }
    return fs::absolute(fs::current_path());
}

static void ensure_setpath_initialized()
{
    using dottalk::paths::state;
    using dottalk::paths::init_defaults;
    using dottalk::paths::Slot;
    using dottalk::paths::get_slot;

    if (state().data_root.empty()) {
        init_defaults(find_data_root_guess());
        return;
    }
    if (get_slot(Slot::DBF).empty() || get_slot(Slot::PROJECTS).empty()) {
        init_defaults(state().data_root);
    }
}

static bool looks_explicit_path(const std::string& s)
{
    if (s.find('/')  != std::string::npos) return true;
    if (s.find('\\') != std::string::npos) return true;
    if (s.size() >= 2 && std::isalpha((unsigned char)s[0]) && s[1] == ':') return true;
    if (!s.empty() && s[0] == '.') return true;
    return false;
}

static fs::path cobol_root()
{
    ensure_setpath_initialized();
    return dottalk::paths::get_slot(dottalk::paths::Slot::PROJECTS) / "cobol";
}

static fs::path cobol_src_root()
{
    return cobol_root() / "src";
}

static fs::path cobol_bin_root()
{
    return cobol_root() / "bin";
}

static fs::path cobol_data_root()
{
    return cobol_root() / "data";
}

// ----------------------- string helpers -------------------------------------

static std::string up_copy(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

static std::string rtrim_copy(std::string s)
{
    while (!s.empty()) {
        const char c = s.back();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') s.pop_back();
        else break;
    }
    return s;
}

static std::string ltrim_copy(std::string s)
{
    std::size_t i = 0;
    while (i < s.size()) {
        const char c = s[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') ++i;
        else break;
    }
    return s.substr(i);
}

static std::string trim_copy(std::string s)
{
    return rtrim_copy(ltrim_copy(std::move(s)));
}

static std::string dequote(std::string s)
{
    s = trim_copy(std::move(s));
    if (s.size() >= 2) {
        const char a = s.front();
        const char b = s.back();
        if ((a == '"' && b == '"') || (a == '\'' && b == '\'')) {
            s = s.substr(1, s.size() - 2);
        }
    }
    return s;
}

static std::string shell_quote(const std::string& s)
{
#if defined(_WIN32)
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else out.push_back(c);
    }
    out += "\"";
    return out;
#else
    std::string out;
    out.reserve(s.size() + 8);
    out.push_back('\'');
    for (char c : s) {
        if (c == '\'') out += "'\"'\"'";
        else out.push_back(c);
    }
    out.push_back('\'');
    return out;
#endif
}

// ----------------------- COBOL path resolution ------------------------------

static fs::path resolve_cobol_source(std::string raw)
{
    raw = dequote(trim_copy(std::move(raw)));
    fs::path p(raw);

    if (looks_explicit_path(raw)) {
        p.make_preferred();
        return p;
    }

    p = cobol_src_root() / p;
    p.make_preferred();
    return p;
}

static fs::path resolve_cobol_program(std::string raw)
{
    raw = dequote(trim_copy(std::move(raw)));
    fs::path p(raw);

    if (looks_explicit_path(raw)) {
        p.make_preferred();
        return p;
    }

    p = cobol_bin_root() / p;
    p.make_preferred();
    return p;
}

static fs::path default_build_output_from_source(const fs::path& source_path)
{
#if defined(_WIN32)
    return cobol_bin_root() / (source_path.stem().string() + ".exe");
#else
    return cobol_bin_root() / source_path.stem();
#endif
}

// ----------------------- STUDENTS export helpers ----------------------------

static bool looks_like_students(const DbArea& db)
{
    if (up_copy(db.logicalName()) != "STUDENTS") {
        return false;
    }

    const auto& f = db.fields();
    if (f.size() != 9) {
        return false;
    }

    return up_copy(f[0].name) == "SID"
        && up_copy(f[1].name) == "LNAME"
        && up_copy(f[2].name) == "FNAME"
        && up_copy(f[3].name) == "DOB"
        && up_copy(f[4].name) == "GENDER"
        && up_copy(f[5].name) == "MAJOR"
        && up_copy(f[6].name) == "ENROLL_D"
        && up_copy(f[7].name) == "GPA"
        && up_copy(f[8].name) == "EMAIL";
}

static bool export_students_for_cobol(DbArea& db, const fs::path& out_file)
{
    std::error_code ec;
    fs::create_directories(out_file.parent_path(), ec);

    std::ofstream out(out_file, std::ios::binary);
    if (!out) {
        std::cout << "COBOL EXPORT: unable to open output file: "
                  << out_file.string() << "\n";
        return false;
    }

    dt::data::Schema schema = dt::data::build_schema_from_dbf(db);

    dt::data::DbfRowCodecOptions dbf_opts;
    std::string dbf_error;
    dt::data::RowSet rowset = dt::data::read_rows_from_dbf(
        db,
        schema,
        /*limit=*/0,
        &dbf_error,
        dbf_opts
    );

    if (!dbf_error.empty()) {
        std::cout << "COBOL EXPORT: " << dbf_error << "\n";
    }

    dt::data::FixedProfile profile = dt::data::build_students_ro_fixed_profile();

    std::string fixed_error;
    const bool ok = dt::data::write_rowset_as_fixed(out, rowset, profile, &fixed_error);
    if (!ok) {
        std::cout << "COBOL EXPORT: " << fixed_error << "\n";
        return false;
    }

    out.flush();

    std::cout << "COBOL EXPORT: wrote "
              << rowset.rows.size()
              << " STUDENTS record(s) to "
              << out_file.string() << "\n";
    return true;
}

// ----------------------- build/run helpers ----------------------------------

static bool has_cobol_source_ext(const fs::path& p)
{
    const std::string ext = up_copy(p.extension().string());
    return ext == ".COB" || ext == ".CBL";
}

static std::string default_cobconfig()
{
    const char* env = std::getenv("COBCONFIG");
    if (env && *env) {
        return std::string(env);
    }

#if defined(_WIN32)
    return "C:\\msys64\\ucrt64\\share\\gnucobol\\config\\default.conf";
#else
    return "";
#endif
}

static int build_cobol_source(const fs::path& raw_source_path, fs::path& out_exe)
{
    std::error_code ec;

    fs::path source_path = raw_source_path;
    source_path.make_preferred();

    fs::create_directories(cobol_src_root(), ec);
    fs::create_directories(cobol_bin_root(), ec);

    if (!fs::exists(source_path)) {
        std::cout << "COBOL BUILD: source not found: " << source_path.string() << "\n";
        return -1;
    }

    if (!fs::is_regular_file(source_path)) {
        std::cout << "COBOL BUILD: not a file: " << source_path.string() << "\n";
        return -1;
    }

    if (!has_cobol_source_ext(source_path)) {
        std::cout << "COBOL BUILD: expected .cob or .cbl source: "
                  << source_path.string() << "\n";
        return -1;
    }

    out_exe = default_build_output_from_source(source_path);

    const std::string cobconfig = default_cobconfig();

    std::string cmd;
#if defined(_WIN32)
    if (!cobconfig.empty()) {
        cmd += "set \"COBCONFIG=" + cobconfig + "\" && ";
    }
    cmd += "cobc -x -free "
        + shell_quote(source_path.string())
        + " -o "
        + shell_quote(out_exe.string());
#else
    if (!cobconfig.empty()) {
        cmd += "COBCONFIG=" + shell_quote(cobconfig) + " ";
    }
    cmd += "cobc -x -free "
        + shell_quote(source_path.string())
        + " -o "
        + shell_quote(out_exe.string());
#endif

    std::cout << "COBOL BUILD: " << source_path.string() << "\n";
    const int rc = std::system(cmd.c_str());

    if (rc == 0) {
        std::cout << "COBOL BUILD: created " << out_exe.string() << "\n";
    } else {
        std::cout << "COBOL BUILD: failed with exit code " << rc << "\n";
    }

    return rc;
}

static int run_program_path(const fs::path& raw_program_path)
{
    std::error_code ec;

    fs::path program_path = raw_program_path;
    program_path.make_preferred();

    fs::create_directories(cobol_bin_root(), ec);

    if (!fs::exists(program_path)) {
        std::cout << "COBOL RUN: file not found: " << program_path.string() << "\n";
        return -1;
    }

    if (!fs::is_regular_file(program_path)) {
        std::cout << "COBOL RUN: not a file: " << program_path.string() << "\n";
        return -1;
    }

    const fs::path old_cwd = fs::current_path();
    const fs::path run_dir = program_path.parent_path().empty()
        ? old_cwd
        : program_path.parent_path();

    fs::current_path(run_dir, ec);
    if (ec) {
        std::cout << "COBOL RUN: unable to switch to directory: "
                  << run_dir.string() << "\n";
        return -1;
    }

    const std::string cmd = shell_quote(program_path.string());
    const int rc = std::system(cmd.c_str());

    fs::current_path(old_cwd, ec);
    return rc;
}

static std::string join_rest(std::istringstream& iss)
{
    std::string rest;
    std::getline(iss, rest);
    return rest;
}

} // namespace

void cmd_COBOL(DbArea& db, std::istringstream& iss)
{
    std::string verb;
    iss >> verb;
    verb = up_copy(verb);

    if (verb.empty()) {
        std::cout
            << "Usage: COBOL EXPORT STUDENTS | COBOL RUN <program> | "
            << "COBOL BUILD <source.cob> | COBOL TEST <source.cob>\n";
        return;
    }

    if (verb == "EXPORT") {
        std::string target;
        iss >> target;
        target = up_copy(target);

        if (target != "STUDENTS") {
            std::cout << "Usage: COBOL EXPORT STUDENTS\n";
            return;
        }

        if (!db.isOpen()) {
            std::cout << "COBOL EXPORT: no table open in current area.\n";
            return;
        }

        if (!looks_like_students(db)) {
            std::cout << "COBOL EXPORT: current area is not the MCC STUDENTS table.\n";
            return;
        }

        const fs::path out_file = cobol_data_root() / "students_ro.dat";
        export_students_for_cobol(db, out_file);
        return;
    }

    if (verb == "RUN") {
        const std::string rest = dequote(join_rest(iss));
        if (rest.empty()) {
            std::cout << "Usage: COBOL RUN <program>\n";
            return;
        }

        const fs::path program_path = resolve_cobol_program(rest);
        const int rc = run_program_path(program_path);
        if (rc >= 0) {
            std::cout << "COBOL RUN: exit code " << rc << "\n";
        }
        return;
    }

    if (verb == "BUILD") {
        const std::string rest = dequote(join_rest(iss));
        if (rest.empty()) {
            std::cout << "Usage: COBOL BUILD <source.cob|source.cbl>\n";
            return;
        }

        fs::path out_exe;
        build_cobol_source(resolve_cobol_source(rest), out_exe);
        return;
    }

    if (verb == "TEST") {
        const std::string rest = dequote(join_rest(iss));
        if (rest.empty()) {
            std::cout << "Usage: COBOL TEST <source.cob|source.cbl>\n";
            return;
        }

        fs::path out_exe;
        const int brc = build_cobol_source(resolve_cobol_source(rest), out_exe);
        if (brc != 0) {
            return;
        }

        const int rrc = run_program_path(out_exe);
        if (rrc >= 0) {
            std::cout << "COBOL RUN: exit code " << rrc << "\n";
        }
        return;
    }

    std::cout
        << "Usage: COBOL EXPORT STUDENTS | COBOL RUN <program> | "
        << "COBOL BUILD <source.cob> | COBOL TEST <source.cob>\n";
}
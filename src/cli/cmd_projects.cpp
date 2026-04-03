#include "cli/cmd_projects.hpp"

#include "cli/cmd_setpath.hpp"
#include "xbase.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

using dottalk::paths::Slot;

enum class ProjectType {
    Data,
    Feature,
    Hybrid
};

struct ManifestInfo {
    std::string name;
    std::string type;
    std::string created;
    std::string engine;
    std::string notes;
};

static std::string trim_copy(std::string s)
{
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };

    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
        [&](unsigned char c) { return !is_space(c); }));

    s.erase(std::find_if(s.rbegin(), s.rend(),
        [&](unsigned char c) { return !is_space(c); }).base(), s.end());

    return s;
}

static std::string upper_copy(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

static bool ieq(const std::string& a, const std::string& b)
{
    return upper_copy(a) == upper_copy(b);
}

static std::vector<std::string> split_tokens(const std::string& s)
{
    std::istringstream iss(s);
    std::vector<std::string> out;
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

static bool is_valid_project_name(const std::string& name)
{
    if (name.empty()) return false;
    if (name == "." || name == "..") return false;

    for (unsigned char ch : name) {
        if (!(std::isalnum(ch) || ch == '_' || ch == '-' || ch == '.'))
            return false;
    }
    return true;
}

static fs::path projects_root()
{
    return dottalk::paths::get_slot(Slot::PROJECTS);
}

static fs::path project_path(const std::string& name)
{
    return projects_root() / name;
}

static fs::path manifest_path_for(const std::string& name)
{
    return project_path(name) / "manifest.txt";
}

static std::string project_type_name(ProjectType t)
{
    switch (t) {
        case ProjectType::Data:    return "DATA";
        case ProjectType::Feature: return "FEATURE";
        case ProjectType::Hybrid:  return "HYBRID";
    }
    return "DATA";
}

static bool parse_project_type(const std::string& s, ProjectType& out)
{
    const std::string u = upper_copy(trim_copy(s));
    if (u.empty() || u == "DATA")    { out = ProjectType::Data;    return true; }
    if (u == "FEATURE")              { out = ProjectType::Feature; return true; }
    if (u == "HYBRID")               { out = ProjectType::Hybrid;  return true; }
    return false;
}

static std::vector<std::string> base_dirs()
{
    return {
        "scripts",
        "tests",
        "logs",
        "tmp"
    };
}

static std::vector<std::string> extra_dirs_for(ProjectType t)
{
    switch (t) {
        case ProjectType::Data:
            return { "dbf", "indexes", "schemas" };
        case ProjectType::Feature:
            return { "notes", "fixtures" };
        case ProjectType::Hybrid:
            return { "dbf", "indexes", "schemas", "notes", "fixtures" };
    }
    return {};
}

static std::vector<std::string> full_dir_set(ProjectType t)
{
    std::set<std::string> uniq;
    for (const auto& s : base_dirs()) uniq.insert(s);
    for (const auto& s : extra_dirs_for(t)) uniq.insert(s);

    return std::vector<std::string>(uniq.begin(), uniq.end());
}

static std::string today_yyyy_mm_dd()
{
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const std::time_t tt = clock::to_time_t(now);

    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &tt);
#else
    localtime_r(&tt, &tmv);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tmv, "%Y-%m-%d");
    return oss.str();
}

static bool write_manifest(const fs::path& manifest_path,
                           const std::string& name,
                           ProjectType type,
                           std::string* err)
{
    std::ofstream out(manifest_path, std::ios::binary);
    if (!out.good()) {
        if (err) *err = "cannot write manifest: " + manifest_path.string();
        return false;
    }

    out << "name: " << name << "\n";
    out << "type: " << project_type_name(type) << "\n";
    out << "created: " << today_yyyy_mm_dd() << "\n";
    out << "engine: DotTalk++\n";
    out << "notes:\n";

    out.flush();
    if (!out.good()) {
        if (err) *err = "failed writing manifest: " + manifest_path.string();
        return false;
    }
    return true;
}

static bool read_manifest(const fs::path& manifest_path, ManifestInfo& out)
{
    std::ifstream in(manifest_path, std::ios::binary);
    if (!in.good()) return false;

    std::string line;
    while (std::getline(in, line)) {
        auto pos = line.find(':');
        if (pos == std::string::npos) continue;

        std::string key = trim_copy(line.substr(0, pos));
        std::string val = trim_copy(line.substr(pos + 1));

        if (ieq(key, "name")) out.name = val;
        else if (ieq(key, "type")) out.type = val;
        else if (ieq(key, "created")) out.created = val;
        else if (ieq(key, "engine")) out.engine = val;
        else if (ieq(key, "notes")) out.notes = val;
    }

    return true;
}

static std::vector<std::string> expected_dirs_from_type_name(const std::string& type_name)
{
    ProjectType t = ProjectType::Data;
    if (!parse_project_type(type_name, t)) {
        return {};
    }
    return full_dir_set(t);
}

static void list_projects()
{
    const fs::path root = projects_root();

    std::error_code ec;
    if (!fs::exists(root, ec) || ec) {
        std::cout << "PROJECTS: root does not exist: " << root.string() << "\n";
        return;
    }

    if (!fs::is_directory(root, ec) || ec) {
        std::cout << "PROJECTS: root is not a directory: " << root.string() << "\n";
        return;
    }

    std::vector<std::string> names;
    for (const auto& de : fs::directory_iterator(root, ec)) {
        if (ec) break;
        if (!de.is_directory()) continue;
        names.push_back(de.path().filename().string());
    }

    std::sort(names.begin(), names.end(),
        [](const std::string& a, const std::string& b) {
            return upper_copy(a) < upper_copy(b);
        });

    std::cout << "PROJECTS:\n";
    if (names.empty()) {
        std::cout << "  (none)\n";
        return;
    }

    for (const auto& n : names) {
        std::cout << "  " << n << "\n";
    }
}

static void create_project(const std::string& name, ProjectType type)
{
    if (!is_valid_project_name(name)) {
        std::cout << "PROJECTS CREATE: invalid project name '" << name << "'.\n";
        std::cout << "Allowed: letters, digits, underscore, dash, dot.\n";
        return;
    }

    const fs::path root = projects_root();
    const fs::path proj = project_path(name);

    std::error_code ec;
    fs::create_directories(root, ec);
    if (ec) {
        std::cout << "PROJECTS CREATE: unable to ensure projects root: "
                  << root.string() << "\n";
        return;
    }

    if (fs::exists(proj, ec)) {
        std::cout << "PROJECTS CREATE: project already exists: " << proj.string() << "\n";
        return;
    }

    fs::create_directories(proj, ec);
    if (ec) {
        std::cout << "PROJECTS CREATE: unable to create project root: "
                  << proj.string() << "\n";
        return;
    }

    bool ok = true;
    for (const auto& dir : full_dir_set(type)) {
        fs::create_directories(proj / dir, ec);
        if (ec) {
            std::cout << "PROJECTS CREATE: failed creating directory: "
                      << (proj / dir).string() << "\n";
            ok = false;
            break;
        }
    }

    if (ok) {
        std::string err;
        if (!write_manifest(proj / "manifest.txt", name, type, &err)) {
            std::cout << "PROJECTS CREATE: " << err << "\n";
            ok = false;
        }
    }

    if (!ok) {
        std::cout << "PROJECTS CREATE: incomplete project creation for '" << name << "'.\n";
        return;
    }

    std::cout << "PROJECTS CREATE: created '" << name << "'"
              << " (" << project_type_name(type) << ")\n";
    std::cout << "  root: " << proj.string() << "\n";
}

static void info_project(const std::string& name)
{
    const fs::path proj = project_path(name);
    const fs::path manifest = manifest_path_for(name);

    std::error_code ec;
    if (!fs::exists(proj, ec) || ec || !fs::is_directory(proj, ec)) {
        std::cout << "PROJECTS INFO: project not found: " << name << "\n";
        return;
    }

    ManifestInfo mi;
    const bool have_manifest = read_manifest(manifest, mi);

    std::cout << "PROJECT: " << name << "\n";
    std::cout << "  Root      : " << proj.string() << "\n";
    std::cout << "  Manifest  : " << (fs::exists(manifest) ? manifest.string() : "(missing)") << "\n";

    if (have_manifest) {
        std::cout << "  Type      : " << (mi.type.empty() ? "(unknown)" : mi.type) << "\n";
        std::cout << "  Created   : " << (mi.created.empty() ? "(unknown)" : mi.created) << "\n";
        std::cout << "  Engine    : " << (mi.engine.empty() ? "(unknown)" : mi.engine) << "\n";
        if (!mi.notes.empty())
            std::cout << "  Notes     : " << mi.notes << "\n";
    } else {
        std::cout << "  Type      : (unknown)\n";
        std::cout << "  Created   : (unknown)\n";
        std::cout << "  Engine    : (unknown)\n";
    }

    std::vector<std::string> expected;
    if (have_manifest && !mi.type.empty()) {
        expected = expected_dirs_from_type_name(mi.type);
    }

    if (!expected.empty()) {
        std::cout << "  Folders   :\n";
        for (const auto& d : expected) {
            const fs::path p = proj / d;
            std::cout << "    " << d << " : "
                      << ((fs::exists(p) && fs::is_directory(p)) ? "present" : "missing")
                      << "\n";
        }
    }
}

static void tree_project(const std::string& name)
{
    const fs::path proj = project_path(name);

    std::error_code ec;
    if (!fs::exists(proj, ec) || ec || !fs::is_directory(proj, ec)) {
        std::cout << "PROJECTS TREE: project not found: " << name << "\n";
        return;
    }

    std::vector<std::string> files;
    std::vector<std::string> dirs;

    for (const auto& de : fs::directory_iterator(proj, ec)) {
        if (ec) break;
        const std::string item = de.path().filename().string();
        if (de.is_directory()) dirs.push_back(item + "/");
        else files.push_back(item);
    }

    auto sorter = [](const std::string& a, const std::string& b) {
        return upper_copy(a) < upper_copy(b);
    };

    std::sort(files.begin(), files.end(), sorter);
    std::sort(dirs.begin(), dirs.end(), sorter);

    std::cout << "PROJECT TREE: " << name << "\n";

    if (files.empty() && dirs.empty()) {
        std::cout << "  (empty)\n";
        return;
    }

    for (const auto& f : files) {
        std::cout << "  " << f << "\n";
    }
    for (const auto& d : dirs) {
        std::cout << "  " << d << "\n";
    }
}

static std::uintmax_t count_entries_recursive(const fs::path& root)
{
    std::error_code ec;
    std::uintmax_t count = 0;

    if (!fs::exists(root, ec) || ec) return 0;

    for (fs::recursive_directory_iterator it(root, ec), end; it != end && !ec; it.increment(ec)) {
        ++count;
    }
    return count;
}

static void delete_project(const std::string& name, bool confirm)
{
    const fs::path proj = project_path(name);

    std::error_code ec;
    if (!fs::exists(proj, ec) || ec || !fs::is_directory(proj, ec)) {
        std::cout << "PROJECTS DELETE: project not found: " << name << "\n";
        return;
    }

    const std::uintmax_t entry_count = count_entries_recursive(proj);

    if (!confirm) {
        std::cout << "PROJECTS DELETE (dry-run): would delete project '" << name << "'\n";
        std::cout << "  root    : " << proj.string() << "\n";
        std::cout << "  entries : " << entry_count << "\n";
        std::cout << "Re-run with CONFIRM to perform deletion.\n";
        return;
    }

    const std::uintmax_t removed = fs::remove_all(proj, ec);
    if (ec) {
        std::cout << "PROJECTS DELETE: failed removing '" << name << "': "
                  << proj.string() << "\n";
        return;
    }

    std::cout << "PROJECTS DELETE: removed '" << name << "'\n";
    std::cout << "  root    : " << proj.string() << "\n";
    std::cout << "  removed : " << removed << "\n";
}

static void print_usage()
{
    std::cout
        << "Usage:\n"
        << "  PROJECTS\n"
        << "  PROJECTS LIST\n"
        << "  PROJECTS CREATE <name> [DATA|FEATURE|HYBRID]\n"
        << "  PROJECTS INFO <name>\n"
        << "  PROJECTS TREE <name>\n"
        << "  PROJECTS DELETE <name> [CONFIRM]\n"
        << "\n"
        << "Notes:\n"
        << "  - Default type is DATA.\n"
        << "  - Base folders are always: scripts, tests, logs, tmp.\n"
        << "  - DATA adds: dbf, indexes, schemas.\n"
        << "  - FEATURE adds: notes, fixtures.\n"
        << "  - HYBRID adds both sets.\n"
        << "  - DELETE is dry-run unless CONFIRM is supplied.\n";
}

} // namespace

void cmd_PROJECTS(xbase::DbArea&, std::istringstream& iss)
{
    std::string rest;
    std::getline(iss, rest);
    rest = trim_copy(rest);

    const std::vector<std::string> toks = split_tokens(rest);

    if (toks.empty()) {
        list_projects();
        return;
    }

    const std::string cmd = upper_copy(toks[0]);

    if (cmd == "LIST") {
        list_projects();
        return;
    }

    if (cmd == "CREATE") {
        if (toks.size() < 2) {
            print_usage();
            return;
        }

        const std::string name = toks[1];
        ProjectType type = ProjectType::Data;

        if (toks.size() >= 3) {
            if (!parse_project_type(toks[2], type)) {
                std::cout << "PROJECTS CREATE: unknown type '" << toks[2] << "'.\n";
                print_usage();
                return;
            }
        }

        create_project(name, type);
        return;
    }

    if (cmd == "INFO") {
        if (toks.size() < 2) {
            print_usage();
            return;
        }
        info_project(toks[1]);
        return;
    }

    if (cmd == "TREE") {
        if (toks.size() < 2) {
            print_usage();
            return;
        }
        tree_project(toks[1]);
        return;
    }

    if (cmd == "DELETE") {
        if (toks.size() < 2) {
            print_usage();
            return;
        }

        bool confirm = false;
        if (toks.size() >= 3 && ieq(toks[2], "CONFIRM")) {
            confirm = true;
        }

        delete_project(toks[1], confirm);
        return;
    }

    print_usage();
}
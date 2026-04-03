// cmd_tablemeta.cpp
// DotTalk++ TABLEMETA command
// Read-only unified metadata report for one DBF table.

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <cctype>

#include "xbase.hpp"
#include "textio.hpp"
#include "cli/path_resolver.hpp"
#include "cli/order_state.hpp"

using namespace std;
namespace fs = std::filesystem;

extern "C" xbase::XBaseEngine* shell_engine();

namespace {

struct IniEntry {
    string key;
    string value;
};

struct IniSection {
    string name;
    vector<IniEntry> entries;
};

static string trim_copy(const string& s)
{
    return textio::trim(s);
}

static string up_copy(string s)
{
    transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return (char)toupper(c); });
    return s;
}

static string low_copy(string s)
{
    transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return (char)tolower(c); });
    return s;
}

static bool iequals(const string& a, const string& b)
{
    return low_copy(a) == low_copy(b);
}

static string yesno(bool v)
{
    return v ? "YES" : "NO";
}

static string exists_text(const fs::path& p)
{
    return fs::exists(p) ? "FOUND" : "MISSING";
}

static uintmax_t safe_file_size(const fs::path& p)
{
    try {
        if (fs::exists(p) && fs::is_regular_file(p)) return fs::file_size(p);
    } catch (...) {}
    return 0;
}

static bool has_memo_fields(xbase::DbArea& a)
{
    try {
        const auto defs = a.fields();
        for (const auto& f : defs) {
            const char t = (char)toupper((unsigned char)f.type);
            if (t == 'M') return true;
        }
    } catch (...) {}
    return false;
}

static fs::path derive_ini_path(const fs::path& dbf)
{
    fs::path p = dbf;
    p.replace_extension(".ini");
    return p;
}

static fs::path derive_dtx_path(const fs::path& dbf)
{
    fs::path p = dbf;
    p.replace_extension(".dtx");
    return p;
}

static fs::path derive_fpt_path(const fs::path& dbf)
{
    fs::path p = dbf;
    p.replace_extension(".fpt");
    return p;
}

static fs::path derive_root_named(const fs::path& dbf, const string& ext)
{
    fs::path p = dbf;
    p.replace_extension(ext);
    return p;
}

static bool load_ini(const fs::path& path,
                     vector<IniSection>& sections,
                     string& error)
{
    sections.clear();
    error.clear();

    ifstream in(path.string());
    if (!in) {
        error = "cannot open file";
        return false;
    }

    string line;
    string current;

    while (getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const string t = trim_copy(line);

        if (t.empty()) continue;
        if (t[0] == ';' || t[0] == '#') continue;

        if (t.front() == '[') {
            if (t.back() != ']') {
                error = "malformed section";
                return false;
            }
            current = trim_copy(t.substr(1, t.size() - 2));
            sections.push_back({current, {}});
            continue;
        }

        const size_t pos = t.find('=');
        if (pos == string::npos) continue;

        const string key = trim_copy(t.substr(0, pos));
        const string val = trim_copy(t.substr(pos + 1));

        if (sections.empty()) {
            sections.push_back({"global", {}});
        }

        sections.back().entries.push_back({key, val});
    }

    return true;
}

static string ini_get(const vector<IniSection>& sections,
                      const string& section,
                      const string& key,
                      const string& def = "")
{
    for (const auto& s : sections) {
        if (!iequals(s.name, section)) continue;
        for (const auto& e : s.entries) {
            if (iequals(e.key, key)) return e.value;
        }
    }
    return def;
}

static int area_slot_of(xbase::DbArea& a)
{
    auto* eng = shell_engine();
    if (!eng) return -1;

    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        if (&eng->area(i) == &a) return i;
    }
    return -1;
}

static xbase::DbArea* find_open_area_by_path(const fs::path& dbf_path)
{
    auto* eng = shell_engine();
    if (!eng) return nullptr;

    string want;
    try { want = fs::weakly_canonical(dbf_path).string(); }
    catch (...) { want = fs::absolute(dbf_path).string(); }

#if defined(_WIN32)
    transform(want.begin(), want.end(), want.begin(),
        [](unsigned char c){ return (char)tolower(c); });
#endif

    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        try {
            xbase::DbArea& A = eng->area(i);
            const string fn = A.filename();
            if (fn.empty()) continue;

            string got;
            try { got = fs::weakly_canonical(fn).string(); }
            catch (...) { got = fs::absolute(fn).string(); }

#if defined(_WIN32)
            transform(got.begin(), got.end(), got.begin(),
                [](unsigned char c){ return (char)tolower(c); });
#endif

            if (got == want) return &A;
        } catch (...) {}
    }
    return nullptr;
}

static void print_kv(const string& k, const string& v)
{
    cout << "  " << left << setw(22) << k << " : " << v << "\n";
}

static void print_section(const string& name)
{
    cout << name << "\n";
    cout << string(40, '-') << "\n";
}

} // namespace

void cmd_TABLEMETA(xbase::DbArea& area, std::istringstream& iss)
{
    string arg;
    iss >> arg;

    bool raw = false;
    fs::path dbf_path;

    if (!arg.empty() && iequals(arg, "RAW")) {
        raw = true;
        arg.clear();
        iss >> arg;
    }

    if (arg.empty()) {
        const string fn = area.filename();
        if (fn.empty()) {
            cout << "TABLEMETA: no table open.\n";
            return;
        }
        dbf_path = fs::path(fn);
    } else if (iequals(arg, "PATH")) {
        string p;
        iss >> p;
        if (p.empty()) {
            cout << "TABLEMETA: PATH requires a DBF path.\n";
            return;
        }
        dbf_path = fs::path(p);
    } else {
        fs::path p(arg);
        if (!p.has_extension()) p.replace_extension(".dbf");
        dbf_path = dottalk::paths::resolve_dbf(p.string());
    }

    xbase::DbArea* openA = find_open_area_by_path(dbf_path);

    bool opened_here = false;
    xbase::DbArea temp;
    xbase::DbArea* metaA = openA;

    if (!metaA) {
        try {
            temp.open(dbf_path.string());
            metaA = &temp;
            opened_here = true;
        } catch (const exception& ex) {
            cout << "TABLEMETA: open failed: " << ex.what() << "\n";
            return;
        }
    }

    const fs::path abs_dbf = fs::absolute(dbf_path);
    const fs::path ini_path = derive_ini_path(abs_dbf);
    const fs::path dtx_path = derive_dtx_path(abs_dbf);
    const fs::path fpt_path = derive_fpt_path(abs_dbf);
    const fs::path inx_path = derive_root_named(abs_dbf, ".inx");
    const fs::path cdx_path = derive_root_named(abs_dbf, ".cdx");
    const fs::path cnx_path = derive_root_named(abs_dbf, ".cnx");

    vector<IniSection> ini_sections;
    string ini_error;
    const bool ini_exists = fs::exists(ini_path);
    const bool ini_ok = ini_exists ? load_ini(ini_path, ini_sections, ini_error) : false;

    const bool hasMemo = has_memo_fields(*metaA);

    if (raw) {
        cout << "dbf=" << abs_dbf.string() << "\n";
        cout << "ini=" << ini_path.string() << "\n";
        cout << "ini_exists=" << (ini_exists ? "true" : "false") << "\n";
        cout << "ini_ok=" << (ini_ok ? "true" : "false") << "\n";
        cout << "dtx=" << dtx_path.string() << "\n";
        cout << "fpt=" << fpt_path.string() << "\n";
        cout << "inx=" << inx_path.string() << "\n";
        cout << "cdx=" << cdx_path.string() << "\n";
        cout << "cnx=" << cnx_path.string() << "\n";
        cout << "rec_count=" << metaA->recCount() << "\n";
        cout << "field_count=" << metaA->fieldCount() << "\n";
        cout << "rec_length=" << metaA->recordLength() << "\n";
        cout << "has_memo=" << (hasMemo ? "true" : "false") << "\n";
        return;
    }

    cout << "TABLE META REPORT\n";
    cout << "File: " << abs_dbf.string() << "\n\n";

    print_section("Table");
    print_kv("Open state", openA ? ("OPEN (area " + to_string(area_slot_of(*openA)) + ")")
                                 : (opened_here ? "TEMP OPENED" : "CLOSED"));
    print_kv("Filename", abs_dbf.filename().string());
    print_kv("Root name", abs_dbf.stem().string());
    print_kv("Path", abs_dbf.string());
    print_kv("Records", to_string(metaA->recCount()));
    print_kv("Fields", to_string(metaA->fieldCount()));
    print_kv("Record length", to_string(metaA->recordLength()));
    print_kv("Has memo field", yesno(hasMemo));
    cout << "\n";

    print_section("INI companion");
    print_kv("Expected path", ini_path.string());
    print_kv("Exists", exists_text(ini_path));
    if (ini_exists) {
        print_kv("Parse status", ini_ok ? "OK" : ("FAILED: " + ini_error));
        if (ini_ok) {
            print_kv("table.id", ini_get(ini_sections, "table", "id"));
            print_kv("table.name", ini_get(ini_sections, "table", "name"));
            print_kv("ownership.owner_id", ini_get(ini_sections, "ownership", "owner_id"));
            print_kv("ownership.root_name", ini_get(ini_sections, "ownership", "root_name"));
            print_kv("schema.field_count", ini_get(ini_sections, "schema", "field_count"));
            print_kv("schema.record_length", ini_get(ini_sections, "schema", "record_length"));
            print_kv("schema.has_memo", ini_get(ini_sections, "schema", "has_memo"));
            print_kv("schema.schema_fingerprint", ini_get(ini_sections, "schema", "schema_fingerprint"));
            print_kv("companions.default_inx", ini_get(ini_sections, "companions", "default_inx"));
            print_kv("companions.default_cdx", ini_get(ini_sections, "companions", "default_cdx"));
            print_kv("companions.default_cnx", ini_get(ini_sections, "companions", "default_cnx"));
            print_kv("companions.default_tag", ini_get(ini_sections, "companions", "default_tag"));
            print_kv("recovery.recreated", ini_get(ini_sections, "recovery", "recreated"));
        }
    }
    cout << "\n";

    print_section("Memo companion");
    print_kv("Memo field present", yesno(hasMemo));
    print_kv("DTX expected", dtx_path.string());
    print_kv("DTX exists", exists_text(dtx_path));
    if (fs::exists(dtx_path)) {
        print_kv("DTX size", to_string(safe_file_size(dtx_path)));
    }
    print_kv("FPT expected", fpt_path.string());
    print_kv("FPT exists", exists_text(fpt_path));
    if (fs::exists(fpt_path)) {
        print_kv("FPT size", to_string(safe_file_size(fpt_path)));
    }
    cout << "\n";

    print_section("Index companions");
    print_kv("INX root-named", inx_path.string());
    print_kv("INX exists", exists_text(inx_path));
    if (fs::exists(inx_path)) print_kv("INX size", to_string(safe_file_size(inx_path)));

    print_kv("CDX root-named", cdx_path.string());
    print_kv("CDX exists", exists_text(cdx_path));
    if (fs::exists(cdx_path)) print_kv("CDX size", to_string(safe_file_size(cdx_path)));

    print_kv("CNX root-named", cnx_path.string());
    print_kv("CNX exists", exists_text(cnx_path));
    if (fs::exists(cnx_path)) print_kv("CNX size", to_string(safe_file_size(cnx_path)));

    if (openA) {
        string orderType = "PHYSICAL";
        if (orderstate::hasOrder(*openA)) {
            if (orderstate::isCdx(*openA)) orderType = "CDX";
            else if (orderstate::isCnx(*openA)) orderType = "CNX";
            else orderType = "INX/OTHER";
        }
        print_kv("Active order type", orderType);
        print_kv("Active order file", orderstate::orderName(*openA));
        print_kv("Active tag", orderstate::activeTag(*openA));
        print_kv("Direction", orderstate::isAscending(*openA) ? "ASC" : "DESC");
    }
    cout << "\n";

    print_section("Warnings");
    bool any_warn = false;

    if (!ini_exists) {
        print_kv("Warning", "INI file is missing");
        any_warn = true;
    } else if (!ini_ok) {
        print_kv("Warning", "INI file exists but failed to parse");
        any_warn = true;
    }

    if (hasMemo && !fs::exists(dtx_path) && !fs::exists(fpt_path)) {
        print_kv("Warning", "Table has memo fields but no memo sidecar found");
        any_warn = true;
    }

    if (openA && orderstate::isCdx(*openA)) {
        const fs::path active = orderstate::orderName(*openA);
        if (!active.empty() && up_copy(active.stem().string()) != up_copy(abs_dbf.stem().string())) {
            print_kv("Warning", "Active CDX root name does not match DBF root name");
            any_warn = true;
        }
    }

    if (!any_warn) {
        print_kv("Status", "No warnings");
    }
}
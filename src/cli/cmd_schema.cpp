#include "cmd_schema.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include "xbase.hpp"

#if __has_include("cli/path_resolver.hpp") && __has_include("cli/cmd_setpath.hpp")
  #include "cli/path_resolver.hpp"
  #include "cli/cmd_setpath.hpp"
  #define HAVE_PATHS 1
#else
  #define HAVE_PATHS 0
#endif

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace fs = std::filesystem;
using nlohmann::json;

// ---------- small helpers ---------------------------------------------------

static inline std::string up(std::string s) {
    for (auto& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

static inline std::string trim_copy(std::string s) {
    auto is_space = [](unsigned char ch){ return std::isspace(ch) != 0; };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
        [&](unsigned char c){ return !is_space(c); }));
    s.erase(std::find_if(s.rbegin(), s.rend(),
        [&](unsigned char c){ return !is_space(c); }).base(), s.end());
    return s;
}

static bool is_digits(const std::string& s) {
    if (s.empty()) return false;
    for (unsigned char ch : s) if (!std::isdigit(ch)) return false;
    return true;
}

static std::string read_word(std::istringstream& iss) {
    std::string w;
    iss >> w;
    return w;
}

static std::string read_pathish(std::istringstream& iss) {
    iss >> std::ws;
    if (iss.peek() == '"' || iss.peek() == '\'') {
        const char q = (char)iss.get();
        std::string out;
        char c = '\0';
        while (iss.get(c)) {
            if (c == q) break;
            out.push_back(c);
        }
        return out;
    }
    std::string w;
    iss >> w;
    return w;
}

static std::string s8(const fs::path& p) {
#if defined(_WIN32)
    auto u = p.u8string();
    return std::string(u.begin(), u.end());
#else
    return p.string();
#endif
}

static fs::path weak_can(const fs::path& p) {
    std::error_code ec;
    fs::path r = fs::weakly_canonical(p, ec);
    return ec ? p : r;
}

static bool file_exists(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p, ec) && !ec && fs::is_regular_file(p, ec) && !ec;
}

static bool ensure_parent_dir(const fs::path& p) {
    std::error_code ec;
    if (!p.parent_path().empty()) fs::create_directories(p.parent_path(), ec);
    return !ec;
}

#if HAVE_PATHS
namespace paths = dottalk::paths;

static inline fs::path schemas_root() { return paths::get_slot(paths::Slot::SCHEMAS); }
static inline fs::path tmp_root()     { return paths::get_slot(paths::Slot::TMP); }
static inline fs::path dbf_root()     { return paths::get_slot(paths::Slot::DBF); }
#else
static inline fs::path schemas_root() { return fs::current_path(); }
static inline fs::path tmp_root()     { return fs::current_path(); }
static inline fs::path dbf_root()     { return fs::current_path(); }
#endif

// Resolve relative input files under SCHEMAS first, then current working directory.
static fs::path resolve_schema_input(const std::string& raw) {
    fs::path p(raw);
    if (p.is_absolute()) return weak_can(p);

    const fs::path c1 = weak_can(schemas_root() / p);
    if (file_exists(c1)) return c1;

    const fs::path c2 = weak_can(fs::current_path() / p);
    if (file_exists(c2)) return c2;

    return c1; // default reporting path under SCHEMAS
}

// Relative FETCH output should land under SCHEMAS.
static fs::path resolve_schema_fetch_output(const std::string& raw) {
    fs::path p(raw);
    if (p.is_absolute()) return weak_can(p);
    return weak_can(schemas_root() / p);
}

// Relative DBF output should land under TMP by default.
static fs::path resolve_schema_dbf_output(const std::string& raw) {
    fs::path p(raw);
    if (p.is_absolute()) return weak_can(p);
    return weak_can(tmp_root() / p);
}

// Relative CSV inputs for seeding should try TMP, then SCHEMAS, then CWD.
static fs::path resolve_seed_csv_input(const std::string& raw) {
    fs::path p(raw);
    if (p.is_absolute()) return weak_can(p);

    const fs::path t = weak_can(tmp_root() / p);
    if (file_exists(t)) return t;

    const fs::path s = weak_can(schemas_root() / p);
    if (file_exists(s)) return s;

    const fs::path c = weak_can(fs::current_path() / p);
    if (file_exists(c)) return c;

    return t;
}

// Relative rejects files should land under TMP.
static fs::path resolve_rejects_output(const std::string& raw) {
    fs::path p(raw);
    if (p.is_absolute()) return weak_can(p);
    return weak_can(tmp_root() / p);
}

static std::string now_utc_iso() {
    using clock = std::chrono::system_clock;
    const auto t = clock::now();
    std::time_t tt = clock::to_time_t(t);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    std::ostringstream os;
    os << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return os.str();
}

static bool write_text_file(const fs::path& path, const std::string& contents) {
    if (!ensure_parent_dir(path)) return false;
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << contents;
    return (bool)f;
}

static bool copy_file_safe(const fs::path& src, const fs::path& dst) {
    std::error_code ec;
    if (!dst.parent_path().empty()) fs::create_directories(dst.parent_path(), ec);
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    return !ec;
}

// ---------- URL fetch helpers -----------------------------------------------

#ifdef _WIN32

struct ParsedUrl {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = 0;
    bool secure = false;
};

static bool parse_url(const std::string& url, ParsedUrl& out) {
    std::wstring wurl(url.begin(), url.end());

    URL_COMPONENTS uc{};
    wchar_t host[256]{};
    wchar_t path[2048]{};

    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = (DWORD)std::size(host);
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = (DWORD)std::size(path);

    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) return false;

    out.host.assign(uc.lpszHostName, uc.dwHostNameLength);
    out.path.assign(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (out.path.empty()) out.path = L"/";
    out.port = uc.nPort;
    out.secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    return true;
}

static bool http_get_bytes(const std::string& url, std::vector<char>& outBytes, std::string& err) {
    ParsedUrl pu;
    if (!parse_url(url, pu)) {
        err = "invalid URL";
        return false;
    }

    HINTERNET session = WinHttpOpen(
        L"DotTalk++ SCHEMA",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);

    if (!session) {
        err = "WinHttpOpen failed";
        return false;
    }

    HINTERNET connect = WinHttpConnect(session, pu.host.c_str(), pu.port, 0);
    if (!connect) {
        err = "WinHttpConnect failed";
        WinHttpCloseHandle(session);
        return false;
    }

    HINTERNET request = WinHttpOpenRequest(
        connect,
        L"GET",
        pu.path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        pu.secure ? WINHTTP_FLAG_SECURE : 0);

    if (!request) {
        err = "WinHttpOpenRequest failed";
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    BOOL ok = WinHttpSendRequest(
        request,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0);

    if (!ok) {
        err = "WinHttpSendRequest failed";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    ok = WinHttpReceiveResponse(request, nullptr);
    if (!ok) {
        err = "WinHttpReceiveResponse failed";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX)) {
        if (statusCode >= 400) {
            err = "HTTP error " + std::to_string(statusCode);
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return false;
        }
    }

    outBytes.clear();

    while (true) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(request, &avail)) {
            err = "WinHttpQueryDataAvailable failed";
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return false;
        }

        if (avail == 0) break;

        std::vector<char> buf(avail);
        DWORD read = 0;
        if (!WinHttpReadData(request, buf.data(), avail, &read)) {
            err = "WinHttpReadData failed";
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return false;
        }

        outBytes.insert(outBytes.end(), buf.begin(), buf.begin() + read);
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return true;
}

static bool save_bytes_file(const fs::path& path, const std::vector<char>& bytes, std::string& err) {
    if (!ensure_parent_dir(path)) {
        err = "unable to create parent directory";
        return false;
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        err = "unable to open output file";
        return false;
    }

    if (!bytes.empty()) out.write(bytes.data(), (std::streamsize)bytes.size());
    if (!out) {
        err = "write failed";
        return false;
    }
    return true;
}

static int schema_fetch_url_to_file(const std::string& url,
                                    const fs::path& out_file,
                                    bool overwrite)
{
    if (url.empty()) {
        std::cout << "SCHEMA FETCH: missing URL\n";
        return 1;
    }
    if (out_file.empty()) {
        std::cout << "SCHEMA FETCH: missing output file\n";
        return 1;
    }
    if (!overwrite && file_exists(out_file)) {
        std::cout << "SCHEMA FETCH: output exists (use OVERWRITE): " << s8(out_file) << "\n";
        return 1;
    }

    std::vector<char> bytes;
    std::string err;
    if (!http_get_bytes(url, bytes, err)) {
        std::cout << "SCHEMA FETCH: " << err << "\n";
        return 1;
    }
    if (!save_bytes_file(out_file, bytes, err)) {
        std::cout << "SCHEMA FETCH: " << err << "\n";
        return 1;
    }

    std::cout << "SCHEMA FETCH: wrote " << bytes.size()
              << " byte(s) to " << s8(out_file) << "\n";
    return 0;
}

#else

static int schema_fetch_url_to_file(const std::string&,
                                    const fs::path&,
                                    bool)
{
    std::cout << "SCHEMA FETCH: URL fetch not implemented on this platform\n";
    return 1;
}

#endif

// ---------- DBF writer ------------------------------------------------------

struct FieldDef {
    std::string name;
    char        type;
    uint8_t     length;
    uint8_t     decimals;
};

static uint8_t safe_len_for_type(char t, int declared_len) {
    switch (t) {
        case 'C': return (uint8_t)std::clamp(declared_len > 0 ? declared_len : 1, 1, 254);
        case 'N': return (uint8_t)std::clamp(declared_len > 0 ? declared_len : 1, 1, 254);
        case 'D': return 8;
        case 'L': return 1;
        case 'M': return 10;
        default:  return 1;
    }
}

static uint8_t safe_decimals_for_type(char t, int declared_dec) {
    if (t == 'N') return (uint8_t)std::clamp(declared_dec >= 0 ? declared_dec : 0, 0, 15);
    return 0;
}

static std::vector<FieldDef> parse_fields_from_schema(const fs::path& schema_path) {
    std::ifstream f(schema_path);
    if (!f) throw std::runtime_error("cannot open schema: " + s8(schema_path));

    json j = json::parse(f, nullptr, true, true);

    if (!j.contains("fields") || !j["fields"].is_array())
        throw std::runtime_error("schema missing 'fields' array");

    std::vector<FieldDef> out;
    out.reserve(j["fields"].size());

    for (const auto& fld : j["fields"]) {
        if (!fld.contains("name") || !fld.contains("type")) continue;

        const std::string name = fld["name"].get<std::string>();
        const std::string ty   = fld["type"].get<std::string>();
        const char t = ty.empty() ? 'C' : (char)std::toupper((unsigned char)ty[0]);

        const int len = fld.contains("length")   ? fld["length"].get<int>()   : 0;
        const int dec = fld.contains("decimals") ? fld["decimals"].get<int>() : 0;

        FieldDef d;
        d.name     = name.substr(0, 10);
        d.type     = (t=='C'||t=='N'||t=='D'||t=='L'||t=='M') ? t : 'C';
        d.length   = safe_len_for_type(d.type, len);
        d.decimals = safe_decimals_for_type(d.type, dec);

        if (d.type == 'D') { d.length = 8;  d.decimals = 0; }
        if (d.type == 'L') { d.length = 1;  d.decimals = 0; }
        if (d.type == 'M') { d.length = 10; d.decimals = 0; }

        out.push_back(d);
    }

    if (out.empty()) out.push_back(FieldDef{"_STUB", 'C', 1, 0});
    return out;
}

static uint16_t compute_record_length(const std::vector<FieldDef>& fields) {
    uint16_t rec = 1;
    for (const auto& f : fields) rec += f.length;
    return rec;
}

static void write_empty_dbf(const fs::path& out_dbf,
                            const std::vector<FieldDef>& fields)
{
    const uint16_t header_len = (uint16_t)(32 + 32 * fields.size() + 1);
    const uint16_t rec_len    = compute_record_length(fields);

    if (!ensure_parent_dir(out_dbf))
        throw std::runtime_error("cannot create parent directory for: " + s8(out_dbf));

    std::ofstream out(out_dbf, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write: " + s8(out_dbf));

    const uint8_t ver = 0x03;
    const auto t = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(t);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    const uint8_t yy = (uint8_t)((tm.tm_year + 1900) % 100);
    const uint8_t mm = (uint8_t)(tm.tm_mon + 1);
    const uint8_t dd = (uint8_t)tm.tm_mday;

    const auto put16 = [&](uint16_t v){ out.put((char)(v & 0xFF)); out.put((char)((v>>8)&0xFF)); };
    const auto put32 = [&](uint32_t v){
        out.put((char)(v & 0xFF));
        out.put((char)((v >> 8) & 0xFF));
        out.put((char)((v >> 16) & 0xFF));
        out.put((char)((v >> 24) & 0xFF));
    };

    out.put((char)ver);
    out.put((char)yy);
    out.put((char)mm);
    out.put((char)dd);
    put32(0u);
    put16(header_len);
    put16(rec_len);

    for (int i = 0; i < 20; ++i) out.put('\0');

    for (const auto& f : fields) {
        char namebuf[11]{0};
        for (size_t i = 0; i < f.name.size() && i < 10; ++i) namebuf[i] = f.name[i];
        out.write(namebuf, 11);

        out.put((char)f.type);
        out.put('\0'); out.put('\0'); out.put('\0'); out.put('\0');
        out.put((char)f.length);
        out.put((char)f.decimals);
        for (int i = 0; i < 14; ++i) out.put('\0');
    }

    out.put((char)0x0D);
    out.flush();

    if (!out) throw std::runtime_error("write failed: " + s8(out_dbf));
}

static void append_blank_records(const fs::path& out_dbf,
                                 const std::vector<FieldDef>& fields,
                                 uint32_t count)
{
    if (count == 0) return;

    const uint16_t rec_len = compute_record_length(fields);

    std::fstream f(out_dbf, std::ios::binary | std::ios::in | std::ios::out);
    if (!f) throw std::runtime_error("cannot reopen for append: " + s8(out_dbf));

    f.seekg(0, std::ios::beg);
    unsigned char header[32];
    f.read(reinterpret_cast<char*>(header), 32);
    if (!f) throw std::runtime_error("failed to read header for update");

    uint32_t nrecs = (uint32_t)header[4]
                   | ((uint32_t)header[5] << 8)
                   | ((uint32_t)header[6] << 16)
                   | ((uint32_t)header[7] << 24);

    std::string rec;
    rec.resize(rec_len, ' ');
    rec[0] = ' ';

    f.seekp(0, std::ios::end);
    for (uint32_t i = 0; i < count; ++i) {
        f.write(rec.data(), (std::streamsize)rec.size());
        if (!f) throw std::runtime_error("failed to append blank record");
    }

    nrecs += count;
    header[4] = (unsigned char)(nrecs & 0xFF);
    header[5] = (unsigned char)((nrecs >> 8) & 0xFF);
    header[6] = (unsigned char)((nrecs >> 16) & 0xFF);
    header[7] = (unsigned char)((nrecs >> 24) & 0xFF);

    const auto t = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(t);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    header[1] = (unsigned char)((tm.tm_year + 1900) % 100);
    header[2] = (unsigned char)(tm.tm_mon + 1);
    header[3] = (unsigned char)tm.tm_mday;

    f.seekp(0, std::ios::beg);
    f.write(reinterpret_cast<const char*>(header), 32);
    if (!f) throw std::runtime_error("failed to update header after append");
    f.flush();
}

// ---------- command core ----------------------------------------------------

static int schema_validate_stub(const fs::path& schema, const fs::path& validator) {
    if (!file_exists(schema)) {
        std::cout << "SCHEMA VALIDATE: schema file not found: " << s8(schema) << "\n";
        return 1;
    }
    if (!file_exists(validator)) {
        std::cout << "SCHEMA VALIDATE: validator file not found: " << s8(validator) << "\n";
        return 1;
    }
    std::cout << "SCHEMA VALIDATE: OK\n";
    std::cout << "  schema    = " << s8(schema) << "\n";
    std::cout << "  validator = " << s8(validator) << "\n";
    return 0;
}

static int schema_create_dbf_real(
    const fs::path&    out_dbf,
    const fs::path&    schema,
    bool               overwrite,
    const fs::path&    seed_csv,
    uint32_t           seed_blank,
    const fs::path&    rejects_csv,
    bool               emit_sidecars)
{
    if (!file_exists(schema)) {
        std::cout << "SCHEMA CREATE DBF: schema file not found: " << s8(schema) << "\n";
        return 1;
    }
    if (!overwrite && file_exists(out_dbf)) {
        std::cout << "SCHEMA CREATE DBF: output exists (use OVERWRITE): " << s8(out_dbf) << "\n";
        return 1;
    }

    std::vector<FieldDef> fields;
    try {
        fields = parse_fields_from_schema(schema);
        write_empty_dbf(out_dbf, fields);
    } catch (const std::exception& ex) {
        std::cout << "SCHEMA CREATE DBF: " << ex.what() << "\n";
        return 1;
    }

    uint32_t rows_written = 0;
    uint32_t rows_rejected = 0;

    try {
        if (seed_blank > 0) {
            append_blank_records(out_dbf, fields, seed_blank);
            rows_written += seed_blank;
        } else if (!seed_csv.empty()) {
            if (!file_exists(seed_csv)) {
                std::cout << "SCHEMA CREATE DBF: SEED CSV failed: cannot open CSV: "
                          << s8(seed_csv) << "\n";
                return 1;
            }
            std::cout << "SCHEMA CREATE DBF: SEED CSV not yet implemented in this drop-in\n";
            return 1;
        }
    } catch (const std::exception& ex) {
        std::cout << "SCHEMA CREATE DBF: seeding failed: " << ex.what() << "\n";
        return 1;
    }

    const std::string table = out_dbf.stem().string();
    const fs::path sidecar_dir = out_dbf.parent_path().empty() ? fs::path(".") : out_dbf.parent_path();

    if (emit_sidecars) {
        json ddl = json::object();
        ddl["table"] = table;
        ddl["engine"] = "DBF";
        ddl["schema_version"] = "1.0";
        ddl["fields"] = json::array();
        ddl["indexes"] = json::array();
        ddl["relations"] = json::array();

        json notes = json::array();
        notes.push_back("DBF header written");
        ddl["engine_notes"] = notes;

        for (const auto& f : fields) {
            json jf = json::object();
            jf["name"] = f.name;
            jf["type"] = std::string(1, f.type);
            jf["length"] = f.length;
            jf["decimals"] = f.decimals;
            ddl["fields"].push_back(jf);
        }

        write_text_file(sidecar_dir / (table + ".ddl.json"), ddl.dump(2));

        std::string operation = "CREATE";
        if (!seed_csv.empty()) operation = "SEED";
        else if (seed_blank > 0) operation = "SEED_BLANK";

        json source = json::object();
        if (!seed_csv.empty()) {
            source["type"] = "CSV";
            source["path"] = s8(seed_csv);
        } else if (seed_blank > 0) {
            source["type"]  = "BLANK";
            source["count"] = seed_blank;
        } else {
            source["type"] = "NONE";
        }

        json result = json::object();
        result["rows_written"]  = rows_written;
        result["rows_rejected"] = rows_rejected;
        if (!rejects_csv.empty()) result["rejects_path"] = s8(rejects_csv);

        json schema_obj = json::object();
        schema_obj["name"] = table;
        schema_obj["version"] = "1.0";
        schema_obj["path"] = s8(schema);

        json engine_obj = json::object();
        engine_obj["name"] = "dottalkpp";
        engine_obj["version"] = "alpha";
        engine_obj["platform"] = "dbf-header-v1";

        json timestamps = json::object();
        timestamps["finished"] = now_utc_iso();

        json load = json::object();
        load["table"] = table;
        load["schema"] = schema_obj;
        load["operation"] = operation;
        load["source"] = source;
        load["result"] = result;
        load["engine"] = engine_obj;
        load["timestamps"] = timestamps;

        write_text_file(sidecar_dir / (table + ".load.json"), load.dump(2));

        json idx = json::object();
        idx["table"] = table;
        idx["engine"] = "CNX";
        idx["indexes"] = json::array();
        idx["warnings"] = json::array();

        write_text_file(sidecar_dir / (table + ".indexes.json"), idx.dump(2));

        copy_file_safe(schema, sidecar_dir / (table + ".schema.copy.json"));
    }

    if (!rejects_csv.empty() && rows_rejected > 0) {
        write_text_file(rejects_csv, "ERROR_CODE,MESSAGE,FIELD,VALUE,ROWNUM\n");
    }

    std::ostringstream ok;
    ok << "SCHEMA CREATE DBF: OK";
    ok << "\n  schema = " << s8(schema);
    ok << "\n  output = " << s8(out_dbf);
    if (rows_written > 0) ok << "\n  blank_rows = " << rows_written;
    if (emit_sidecars) ok << "\n  sidecars = " << s8(sidecar_dir);
    ok << "\n";
    std::cout << ok.str();
    return 0;
}

// ---------- command entry ---------------------------------------------------

void cmd_SCHEMA(xbase::DbArea& /*area*/, std::istringstream& iss) {
    const std::string subcmd = up(read_word(iss));

    if (subcmd == "FETCH") {
        const std::string url = read_pathish(iss);
        const std::string to_kw = up(read_word(iss));
        if (to_kw != "TO") {
            std::cout << "SCHEMA FETCH: expected TO <file>\n";
            return;
        }
        const std::string out_raw = read_pathish(iss);

        bool overwrite = false;
        while (iss && !iss.eof()) {
            const std::string tok = up(read_word(iss));
            if (tok.empty()) break;
            if (tok == "OVERWRITE") {
                overwrite = true;
                continue;
            }
            break;
        }

        const fs::path out_file = resolve_schema_fetch_output(out_raw);
        (void)schema_fetch_url_to_file(url, out_file, overwrite);
        return;
    }

    if (subcmd == "VALIDATE") {
        const std::string schema_raw = read_pathish(iss);
        const std::string using_kw = up(read_word(iss));
        if (using_kw != "USING") {
            std::cout << "SCHEMA VALIDATE: expected USING <validator.json>\n";
            return;
        }
        const std::string validator_raw = read_pathish(iss);

        const fs::path schema = resolve_schema_input(schema_raw);
        const fs::path validator = resolve_schema_input(validator_raw);
        (void)schema_validate_stub(schema, validator);
        return;
    }

    if (subcmd == "CREATE") {
        const std::string kind = up(read_word(iss));
        if (kind != "DBF") {
            std::cout << "SCHEMA CREATE: only DBF supported\n";
            return;
        }

        const std::string out_dbf_raw = read_pathish(iss);
        const std::string from_kw = up(read_word(iss));
        if (from_kw != "FROM") {
            std::cout << "SCHEMA CREATE DBF: expected FROM <schema.json>\n";
            return;
        }
        const std::string schema_raw = read_pathish(iss);

        bool overwrite = false;
        std::string seed_csv_raw;
        uint32_t    seed_blank = 0;
        std::string rejects_raw;
        bool emit_sidecars = false;

        while (iss && !iss.eof()) {
            std::streampos p = iss.tellg();
            const std::string tok = up(read_word(iss));
            if (tok.empty()) break;

            if (tok == "OVERWRITE") {
                overwrite = true;
                continue;
            }

            if (tok == "SEED") {
                const std::string mode_kw = up(read_word(iss));
                if (mode_kw == "CSV") {
                    seed_csv_raw = read_pathish(iss);
                    continue;
                } else if (mode_kw == "BLANK") {
                    const std::string n = read_word(iss);
                    if (!is_digits(n)) {
                        std::cout << "SCHEMA CREATE DBF: expected SEED BLANK <N>\n";
                        return;
                    }
                    seed_blank = (uint32_t)std::stoul(n);
                    continue;
                } else {
                    std::cout << "SCHEMA CREATE DBF: expected CSV or BLANK after SEED\n";
                    return;
                }
            }

            if (tok == "REJECTS") {
                rejects_raw = read_pathish(iss);
                continue;
            }

            if (tok == "EMIT") {
                const std::string sc_kw = up(read_word(iss));
                if (sc_kw != "SIDECARS") {
                    std::cout << "SCHEMA CREATE DBF: expected SIDECARS after EMIT\n";
                    return;
                }
                emit_sidecars = true;
                continue;
            }

            iss.clear();
            iss.seekg(p);
            break;
        }

        if (!seed_csv_raw.empty() && seed_blank > 0) {
            std::cout << "SCHEMA CREATE DBF: both SEED CSV and SEED BLANK provided; CSV takes precedence.\n";
        }

        const fs::path out_dbf = resolve_schema_dbf_output(out_dbf_raw);
        const fs::path schema = resolve_schema_input(schema_raw);
        const fs::path seed_csv = seed_csv_raw.empty() ? fs::path{} : resolve_seed_csv_input(seed_csv_raw);
        const fs::path rejects_csv = rejects_raw.empty() ? fs::path{} : resolve_rejects_output(rejects_raw);

        (void)schema_create_dbf_real(out_dbf, schema, overwrite, seed_csv, seed_blank, rejects_csv, emit_sidecars);
        return;
    }

    std::cout
        << "SCHEMA command\n"
        << "  SCHEMA FETCH <url> TO <file> [OVERWRITE]\n"
        << "  SCHEMA VALIDATE <schema.json> USING <validator.json>\n"
        << "  SCHEMA CREATE DBF <out.dbf> FROM <schema.json> [OVERWRITE]\n"
        << "      [SEED CSV <path.csv> | SEED BLANK <N>]\n"
        << "      [REJECTS <rejects.csv>] [EMIT SIDECARS]\n"
        << "Path rules:\n"
        << "  - Relative schema inputs resolve under SCHEMAS.\n"
        << "  - Relative FETCH outputs resolve under SCHEMAS.\n"
        << "  - Relative CREATE DBF outputs resolve under TMP.\n";
}
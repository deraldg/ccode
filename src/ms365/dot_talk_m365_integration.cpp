// dot_talk_m365_integration.cpp
// Milestone 1: Primitive implementation skeleton

#include "dot_talk_m365_integration.hpp"

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <vector>

namespace fs = std::filesystem;

namespace dottalk::m365
{
    // ------------------------------------------------------------
    // Internal state
    // ------------------------------------------------------------

    namespace
    {
        std::string g_oneDriveRoot;
        bool        g_oneDriveInitialized = false;

        bool ensure_folder(const fs::path& p)
        {
            std::error_code ec;
            if (fs::exists(p, ec))
                return fs::is_directory(p, ec);

            return fs::create_directories(p, ec) && !ec;
        }

        std::string get_env(const char* name)
        {
            const char* v = std::getenv(name);
            return v ? std::string(v) : std::string{};
        }

        bool detect_onedrive_root()
        {
            if (g_oneDriveInitialized)
                return !g_oneDriveRoot.empty();

            // 1) Explicit override
            std::string overrideRoot = get_env("DOTTALK_ONEDRIVE_ROOT");
            if (!overrideRoot.empty() && fs::exists(overrideRoot))
            {
                g_oneDriveRoot = overrideRoot;
                g_oneDriveInitialized = true;
                return true;
            }

            // 2) OneDrive environment variables
            std::vector<std::string> candidates;
            for (auto name : { "ONEDRIVE", "ONEDRIVE_COMMERCIAL", "ONEDRIVE_CONSUMER" })
            {
                auto v = get_env(name);
                if (!v.empty())
                    candidates.push_back(v);
            }

#ifdef _WIN32
            // 3) Fallback patterns on Windows
            auto userProfile = get_env("USERPROFILE");
            if (!userProfile.empty())
            {
                candidates.push_back(userProfile + "\\OneDrive");
            }
#endif

            for (const auto& c : candidates)
            {
                std::error_code ec;
                if (!c.empty() && fs::exists(c, ec) && fs::is_directory(c, ec))
                {
                    g_oneDriveRoot = c;
                    g_oneDriveInitialized = true;
                    return true;
                }
            }

            g_oneDriveInitialized = true;
            g_oneDriveRoot.clear();
            return false;
        }

        fs::path root_path()
        {
            if (!detect_onedrive_root())
                return fs::path(); // empty
            return fs::path(g_oneDriveRoot);
        }

        fs::path exchange_root()
        {
            auto r = root_path();
            if (r.empty())
                return fs::path();
            return r / Paths::Root;
        }

        fs::path subfolder_path(const char* subfolder)
        {
            auto r = root_path();
            if (r.empty())
                return fs::path();
            // subfolder is relative to OneDrive root (e.g. "DotTalk_Exchange/outbound")
            return r / subfolder;
        }

        std::string make_filename_timestamp()
        {
            using clock = std::chrono::system_clock;
            auto now = clock::now();
            auto t   = clock::to_time_t(now);
            std::tm tm{};
#ifdef _WIN32
            gmtime_s(&tm, &t);
#else
            gmtime_r(&t, &tm);
#endif
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
            return oss.str();
        }

        Timestamp make_iso_timestamp()
        {
            using clock = std::chrono::system_clock;
            auto now = clock::now();
            auto t   = clock::to_time_t(now);
            std::tm tm{};
#ifdef _WIN32
            gmtime_s(&tm, &t);
#else
            gmtime_r(&t, &tm);
#endif
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
            return Timestamp{ oss.str() };
        }

        FileKind classify_kind(const std::string& name)
        {
            if (name.rfind(Naming::TablePrefixOutbound, 0) == 0 ||
                name.rfind(Naming::TablePrefixInbound, 0) == 0)
            {
                if (name.size() >= 4 &&
                    name.compare(name.size() - 4, 4, Naming::ExtCSV) == 0)
                    return FileKind::TableCSV;
            }

            if (name.rfind(Naming::RecordPrefixOutbound, 0) == 0 ||
                name.rfind(Naming::RecordPrefixInbound, 0) == 0)
            {
                if (name.size() >= 5 &&
                    name.compare(name.size() - 5, 5, Naming::ExtJSON) == 0)
                    return FileKind::RecordJSON;
            }

            if (name.rfind(Naming::NotesPrefixOutbound, 0) == 0 ||
                name.rfind(Naming::NotesPrefixInbound, 0) == 0)
            {
                if (name.size() >= 4 &&
                    name.compare(name.size() - 4, 4, Naming::ExtTXT) == 0)
                    return FileKind::NotesTXT;
            }

            return FileKind::Unknown;
        }

        Direction classify_direction(const std::string& name)
        {
            if (name.rfind("import_", 0) == 0)
                return Direction::Inbound;
            return Direction::Outbound;
        }

        Timestamp file_timestamp_from_fs(const fs::path& p)
        {
            std::error_code ec;
            auto ftime = fs::last_write_time(p, ec);
            if (ec)
                return make_iso_timestamp();

            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - fs::file_time_type::clock::now()
                + std::chrono::system_clock::now());

            auto t = std::chrono::system_clock::to_time_t(sctp);
            std::tm tm{};
#ifdef _WIN32
            gmtime_s(&tm, &t);
#else
            gmtime_r(&t, &tm);
#endif
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
            return Timestamp{ oss.str() };
        }
    } // anonymous namespace

    // ------------------------------------------------------------
    // Public helpers
    // ------------------------------------------------------------

    Timestamp make_current_timestamp_utc()
    {
        return make_iso_timestamp();
    }

    std::string build_path(const std::string& subfolder, const std::string& file_name)
    {
        auto r = root_path();
        if (r.empty())
            return {};

        fs::path p = r / subfolder / file_name;
        return p.string();
    }

    FileDescriptor classify_file(const std::string& full_path, const std::string& file_name)
    {
        FileDescriptor fd;
        fd.full_path = full_path;
        fd.file_name = file_name;
        fd.kind      = classify_kind(file_name);
        fd.direction = classify_direction(file_name);
        fd.created   = file_timestamp_from_fs(fs::path(full_path));
        return fd;
    }

    // ------------------------------------------------------------
    // Export implementations (Milestone 1: placeholder content)
    // ------------------------------------------------------------

    bool export_table_to_csv(
        const std::string& table_name,
        const Timestamp&   ts,
        std::string&       out_full_path)
    {
        (void)ts;
        auto outDir = subfolder_path(Paths::Outbound);
        if (outDir.empty() || !ensure_folder(outDir))
            return false;

        std::string fname = std::string(Naming::TablePrefixOutbound)
                          + table_name + "_" + make_filename_timestamp()
                          + Naming::ExtCSV;

        fs::path full = outDir / fname;

        std::ofstream ofs(full, std::ios::binary);
        if (!ofs)
            return false;

        // Placeholder CSV content for Milestone 1
        ofs << "placeholder_column\n";
        ofs << "placeholder_value\n";

        ofs.close();
        out_full_path = full.string();
        return true;
    }

    bool export_record_to_json(
        const std::string& table_name,
        std::int64_t       record_id,
        const Timestamp&   ts,
        std::string&       out_full_path)
    {
        auto outDir = subfolder_path(Paths::Outbound);
        if (outDir.empty() || !ensure_folder(outDir))
            return false;

        std::ostringstream idss;
        idss << record_id;

        std::string fname = std::string(Naming::RecordPrefixOutbound)
                          + table_name + "_" + idss.str()
                          + Naming::ExtJSON;

        fs::path full = outDir / fname;

        std::ofstream ofs(full, std::ios::binary);
        if (!ofs)
            return false;

        // Placeholder JSON content for Milestone 1
        ofs << "{\n";
        ofs << "  \"table\": \"" << table_name << "\",\n";
        ofs << "  \"id\": " << record_id << ",\n";
        ofs << "  \"metadata\": {\n";
        ofs << "    \"last_update\": \"" << ts.iso_8601 << "\",\n";
        ofs << "    \"source\": \"dottalk\"\n";
        ofs << "  }\n";
        ofs << "}\n";

        ofs.close();
        out_full_path = full.string();
        return true;
    }

    bool export_notes_to_txt(
        const std::string& topic,
        const Timestamp&   ts,
        std::string&       out_full_path)
    {
        auto outDir = subfolder_path(Paths::Outbound);
        if (outDir.empty() || !ensure_folder(outDir))
            return false;

        std::string fname = std::string(Naming::NotesPrefixOutbound)
                          + topic + "_" + make_filename_timestamp()
                          + Naming::ExtTXT;

        fs::path full = outDir / fname;

        std::ofstream ofs(full, std::ios::binary);
        if (!ofs)
            return false;

        // Placeholder TXT content for Milestone 1
        ofs << "Notes topic: " << topic << "\n";
        ofs << "Created: " << ts.iso_8601 << "\n";
        ofs << "\n";
        ofs << "This is placeholder content generated by DotTalk++.\n";

        ofs.close();
        out_full_path = full.string();
        return true;
    }

    // ------------------------------------------------------------
    // Inbound scan
    // ------------------------------------------------------------

    InboundScanResult scan_inbound_folder()
    {
        InboundScanResult result;

        auto inDir = subfolder_path(Paths::Inbound);
        if (inDir.empty())
        {
            result.ok = false;
            result.error_message = "OneDrive root not found.";
            return result;
        }

        if (!ensure_folder(inDir))
        {
            result.ok = false;
            result.error_message = "Failed to create or access inbound folder.";
            return result;
        }

        std::error_code ec;
        for (auto& entry : fs::directory_iterator(inDir, ec))
        {
            if (ec)
                break;

            if (!entry.is_regular_file())
                continue;

            auto p  = entry.path();
            auto fn = p.filename().string();

            FileDescriptor fd = classify_file(p.string(), fn);
            if (fd.kind == FileKind::Unknown)
                continue;

            fd.direction = Direction::Inbound;
            result.files.push_back(std::move(fd));
        }

        if (ec)
        {
            result.ok = false;
            result.error_message = "Error scanning inbound folder.";
            return result;
        }

        result.ok = true;
        return result;
    }

    // ------------------------------------------------------------
    // Import stubs (Milestone 1: no real data application)
    // ------------------------------------------------------------

    bool import_table_from_csv(
        const FileDescriptor& file,
        std::string&          out_table_name)
    {
        // Milestone 1: just parse name and confirm file exists
        fs::path p(file.full_path);
        if (!fs::exists(p))
            return false;

        // crude extraction: table_<name>_... or import_table_<name>.csv
        std::string name = file.file_name;
        std::string prefix = Naming::TablePrefixInbound;
        auto pos = name.find(prefix);
        if (pos == std::string::npos)
            prefix = Naming::TablePrefixOutbound;

        pos = name.find(prefix);
        if (pos != std::string::npos)
        {
            auto start = pos + prefix.size();
            auto end   = name.find('_', start);
            if (end == std::string::npos)
                end = name.find('.', start);
            if (end != std::string::npos)
                out_table_name = name.substr(start, end - start);
        }

        return true;
    }

    bool import_record_from_json(
        const FileDescriptor& file,
        std::string&          out_table_name,
        std::int64_t&         out_record_id)
    {
        fs::path p(file.full_path);
        if (!fs::exists(p))
            return false;

        // Milestone 1: parse table + id from filename
        // record_<table>_<id>.json or import_record_<table>_<id>.json
        std::string name = file.file_name;
        std::string prefix = Naming::RecordPrefixInbound;
        auto pos = name.find(prefix);
        if (pos == std::string::npos)
            prefix = Naming::RecordPrefixOutbound;

        pos = name.find(prefix);
        if (pos != std::string::npos)
        {
            auto start = pos + prefix.size();
            auto mid   = name.find('_', start);
            auto end   = name.find('.', mid + 1);

            if (mid != std::string::npos && end != std::string::npos)
            {
                out_table_name = name.substr(start, mid - start);
                auto idStr = name.substr(mid + 1, end - (mid + 1));
                try
                {
                    out_record_id = std::stoll(idStr);
                }
                catch (...)
                {
                    out_record_id = 0;
                }
            }
        }

        return true;
    }

    bool import_notes_from_txt(
        const FileDescriptor& file,
        std::string&          out_topic)
    {
        fs::path p(file.full_path);
        if (!fs::exists(p))
            return false;

        // Milestone 1: parse topic from filename
        // notes_<topic>_... or import_notes_<topic>.txt
        std::string name = file.file_name;
        std::string prefix = Naming::NotesPrefixInbound;
        auto pos = name.find(prefix);
        if (pos == std::string::npos)
            prefix = Naming::NotesPrefixOutbound;

        pos = name.find(prefix);
        if (pos != std::string::npos)
        {
            auto start = pos + prefix.size();
            auto end   = name.find('_', start);
            if (end == std::string::npos)
                end = name.find('.', start);
            if (end != std::string::npos)
                out_topic = name.substr(start, end - start);
        }

        return true;
    }

    // ------------------------------------------------------------
    // Archiving
    // ------------------------------------------------------------

    bool archive_file(
        const FileDescriptor& file,
        std::string&          out_archived_path)
    {
        auto archRoot = subfolder_path(Paths::Archive);
        if (archRoot.empty() || !ensure_folder(archRoot))
            return false;

        // derive year/month from timestamp
        if (file.created.iso_8601.size() < 7)
            return false;

        std::string year  = file.created.iso_8601.substr(0, 4);
        std::string month = file.created.iso_8601.substr(5, 2);

        fs::path yearDir  = fs::path(archRoot) / year;
        fs::path monthDir = yearDir / month;

        if (!ensure_folder(yearDir) || !ensure_folder(monthDir))
            return false;

        fs::path src = file.full_path;
        fs::path dst = monthDir / file.file_name;

        std::error_code ec;
        fs::rename(src, dst, ec);
        if (ec)
        {
            // fallback: copy + remove
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
            if (ec)
                return false;
            fs::remove(src, ec);
            if (ec)
                return false;
        }

        out_archived_path = dst.string();
        return true;
    }

} // namespace dottalk::m365
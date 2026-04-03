// dot_talk_m365_integration.hpp
// Primitive file-based integration with Microsoft 365 via OneDrive
// Version: 0.1.0

#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace dottalk::m365
{
    struct Paths
    {
        static constexpr const char* Root     = "DotTalk_Exchange";
        static constexpr const char* Inbound  = "DotTalk_Exchange/inbound";
        static constexpr const char* Outbound = "DotTalk_Exchange/outbound";
        static constexpr const char* Archive  = "DotTalk_Exchange/archive";
    };

    struct Naming
    {
        static constexpr const char* TablePrefixOutbound  = "table_";
        static constexpr const char* RecordPrefixOutbound = "record_";
        static constexpr const char* NotesPrefixOutbound  = "notes_";

        static constexpr const char* TablePrefixInbound   = "import_table_";
        static constexpr const char* RecordPrefixInbound  = "import_record_";
        static constexpr const char* NotesPrefixInbound   = "import_notes_";

        static constexpr const char* ExtCSV  = ".csv";
        static constexpr const char* ExtJSON = ".json";
        static constexpr const char* ExtTXT  = ".txt";
    };

    enum class FileKind : std::uint8_t
    {
        Unknown = 0,
        TableCSV,
        RecordJSON,
        NotesTXT
    };

    enum class Direction : std::uint8_t
    {
        Outbound = 0,
        Inbound
    };

    struct Timestamp
    {
        std::string iso_8601;
    };

    struct FileDescriptor
    {
        std::string full_path;
        std::string file_name;
        FileKind    kind{ FileKind::Unknown };
        Direction   direction{ Direction::Outbound };
        Timestamp   created;
    };

    struct InboundScanResult
    {
        bool                        ok{ false };
        std::string                 error_message;
        std::vector<FileDescriptor> files;
    };

    // Export
    bool export_table_to_csv(
        const std::string& table_name,
        const Timestamp&   ts,
        std::string&       out_full_path);

    bool export_record_to_json(
        const std::string& table_name,
        std::int64_t       record_id,
        const Timestamp&   ts,
        std::string&       out_full_path);

    bool export_notes_to_txt(
        const std::string& topic,
        const Timestamp&   ts,
        std::string&       out_full_path);

    // Import
    InboundScanResult scan_inbound_folder();

    bool import_table_from_csv(
        const FileDescriptor& file,
        std::string&          out_table_name);

    bool import_record_from_json(
        const FileDescriptor& file,
        std::string&          out_table_name,
        std::int64_t&         out_record_id);

    bool import_notes_from_txt(
        const FileDescriptor& file,
        std::string&          out_topic);

    // Archive
    bool archive_file(
        const FileDescriptor& file,
        std::string&          out_archived_path);

    // Helpers
    Timestamp    make_current_timestamp_utc();
    std::string  build_path(const std::string& subfolder, const std::string& file_name);
    FileDescriptor classify_file(const std::string& full_path, const std::string& file_name);

} // namespace dottalk::m365
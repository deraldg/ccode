#include "cmd_importsql.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "import/import_normalize.hpp"
#include "import/import_profile.hpp"
#include "xbase.hpp"

using dottalkpp::import::ColumnProfile;
using dottalkpp::import::classify_profile;
using dottalkpp::import::normalize_field_name;
using dottalkpp::import::proposed_target_decimals;
using dottalkpp::import::proposed_target_type;
using dottalkpp::import::proposed_target_width;
using dottalkpp::import::update_profile;

namespace
{
    struct HeaderIssue
    {
        bool hasEmptyHeaders = false;
        bool hasDuplicateNormalizedHeaders = false;
        std::vector<std::size_t> emptyHeaderIndexes;
        std::vector<std::string> duplicateNormalizedNames;
    };

    struct ValidateResult
    {
        bool ok = true;
        std::size_t headerColumnCount = 0;
        std::size_t rowsChecked = 0;
        std::size_t firstBadLine = 0;
        std::size_t expectedColumns = 0;
        std::size_t actualColumns = 0;

        std::vector<std::string> headers;
        std::vector<std::string> normalizedHeaders;
        std::vector<ColumnProfile> profiles;

        HeaderIssue headerIssue;
    };

    std::vector<std::string> split_simple(const std::string& line, char delim)
    {
        std::vector<std::string> parts;
        std::string current;

        for (char ch : line)
        {
            if (ch == delim)
            {
                parts.push_back(current);
                current.clear();
            }
            else
            {
                current += ch;
            }
        }

        parts.push_back(current);
        return parts;
    }

    char detect_delimiter_from_name(const std::string& name)
    {
        const std::string upper = normalize_field_name(name);

        if (upper == "TAB")
            return '\t';
        if (upper == "COMMA")
            return ',';

        return '|';
    }

    std::string delimiter_label(char delim)
    {
        switch (delim)
        {
        case '|':  return "PIPE";
        case '\t': return "TAB";
        case ',':  return "COMMA";
        default:   return std::string(1, delim);
        }
    }

    bool open_text_file(const std::string& filePath, std::ifstream& in)
    {
        in.open(filePath, std::ios::in | std::ios::binary);
        return static_cast<bool>(in);
    }

    void strip_trailing_cr(std::string& line)
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
    }

    HeaderIssue analyze_header(const std::vector<std::string>& headers)
    {
        HeaderIssue issue;
        std::unordered_map<std::string, int> seenNormalized;

        for (std::size_t i = 0; i < headers.size(); ++i)
        {
            const std::string& raw = headers[i];
            const std::string normalized = normalize_field_name(raw);

            if (raw.empty())
            {
                issue.hasEmptyHeaders = true;
                issue.emptyHeaderIndexes.push_back(i + 1);
            }

            auto it = seenNormalized.find(normalized);
            if (it == seenNormalized.end())
            {
                seenNormalized.emplace(normalized, 1);
            }
            else
            {
                ++(it->second);
                issue.hasDuplicateNormalizedHeaders = true;

                bool alreadyListed = false;
                for (const std::string& dup : issue.duplicateNormalizedNames)
                {
                    if (dup == normalized)
                    {
                        alreadyListed = true;
                        break;
                    }
                }

                if (!alreadyListed)
                    issue.duplicateNormalizedNames.push_back(normalized);
            }
        }

        return issue;
    }

    void print_header_warnings(const HeaderIssue& issue)
    {
        if (!issue.hasEmptyHeaders && !issue.hasDuplicateNormalizedHeaders)
            return;

        std::cout << "\n";
        std::cout << "Warnings\n";

        if (issue.hasEmptyHeaders)
        {
            std::cout << "  Empty header field(s) at column(s): ";
            for (std::size_t i = 0; i < issue.emptyHeaderIndexes.size(); ++i)
            {
                if (i > 0)
                    std::cout << ", ";
                std::cout << issue.emptyHeaderIndexes[i];
            }
            std::cout << "\n";
        }

        if (issue.hasDuplicateNormalizedHeaders)
        {
            std::cout << "  Duplicate normalized header name(s): ";
            for (std::size_t i = 0; i < issue.duplicateNormalizedNames.size(); ++i)
            {
                if (i > 0)
                    std::cout << ", ";
                std::cout << issue.duplicateNormalizedNames[i];
            }
            std::cout << "\n";
        }
    }

    void print_columns_report(const std::vector<std::string>& headers,
                              const std::vector<std::string>& normalizedHeaders,
                              const std::vector<ColumnProfile>* profiles)
    {
        std::cout << "\n";
        std::cout << "Columns\n";

        for (std::size_t i = 0; i < headers.size(); ++i)
        {
            std::cout << "  "
                      << (i + 1)
                      << "  "
                      << headers[i]
                      << " -> "
                      << normalizedHeaders[i];

            if (profiles != nullptr && i < profiles->size())
            {
                const ColumnProfile& p = (*profiles)[i];
                const std::string klass = classify_profile(p);
                const char targetType = proposed_target_type(p);
                const int targetWidth = proposed_target_width(p);
                const int targetDec = proposed_target_decimals(p);

                std::cout << "  "
                          << klass
                          << "  "
                          << "max=" << p.maxLen
                          << "  "
                          << "target=" << targetType;

                if (targetType == 'N')
                {
                    std::cout << "(" << targetWidth << "," << targetDec << ")";
                }
                else if (targetType == 'C')
                {
                    std::cout << "(" << targetWidth << ")";
                }
            }

            std::cout << "\n";
        }
    }

    void print_schema_report(const ValidateResult& result)
    {
        std::cout << "\n";
        std::cout << "Proposed DBF Schema\n";

        for (std::size_t i = 0; i < result.normalizedHeaders.size(); ++i)
        {
            const std::string& name = result.normalizedHeaders[i];
            const ColumnProfile& p = result.profiles[i];
            const char targetType = proposed_target_type(p);
            const int targetWidth = proposed_target_width(p);
            const int targetDec = proposed_target_decimals(p);

            std::cout << "  " << name << "  " << targetType;

            if (targetType == 'N')
            {
                std::cout << "(" << targetWidth << "," << targetDec << ")";
            }
            else if (targetType == 'C')
            {
                std::cout << "(" << targetWidth << ")";
            }

            std::cout << "\n";
        }
    }

    void print_create_report(const ValidateResult& result, const std::string& tableName)
    {
        std::cout << "\n";
        std::cout << "Proposed CREATE command\n\n";
        std::cout << "CREATE " << tableName << " (\n";

        for (std::size_t i = 0; i < result.normalizedHeaders.size(); ++i)
        {
            const std::string& name = result.normalizedHeaders[i];
            const ColumnProfile& p = result.profiles[i];
            const char targetType = proposed_target_type(p);
            const int targetWidth = proposed_target_width(p);
            const int targetDec = proposed_target_decimals(p);

            std::cout << "  " << name << " " << targetType;

            if (targetType == 'N')
            {
                std::cout << "(" << targetWidth << "," << targetDec << ")";
            }
            else if (targetType == 'C')
            {
                std::cout << "(" << targetWidth << ")";
            }

            if (i + 1 < result.normalizedHeaders.size())
                std::cout << ",";

            std::cout << "\n";
        }

        std::cout << ")\n";
    }

    ValidateResult run_profile_scan(const std::string& filePath, char delim)
    {
        ValidateResult result;

        if (filePath.empty())
        {
            std::cout << "IMPORTSQL requires a file path\n";
            result.ok = false;
            return result;
        }

        std::ifstream in;
        if (!open_text_file(filePath, in))
        {
            std::cout << "Unable to open file: " << filePath << "\n";
            result.ok = false;
            return result;
        }

        std::string headerLine;
        if (!std::getline(in, headerLine))
        {
            std::cout << "File is empty: " << filePath << "\n";
            result.ok = false;
            return result;
        }

        strip_trailing_cr(headerLine);

        result.headers = split_simple(headerLine, delim);
        result.headerColumnCount = result.headers.size();
        result.expectedColumns = result.headers.size();

        result.normalizedHeaders.reserve(result.headers.size());
        for (const std::string& h : result.headers)
            result.normalizedHeaders.push_back(normalize_field_name(h));

        result.profiles.resize(result.headers.size());

        result.headerIssue = analyze_header(result.headers);

        if (result.headerIssue.hasEmptyHeaders ||
            result.headerIssue.hasDuplicateNormalizedHeaders)
        {
            result.ok = false;
        }

        std::string line;
        std::size_t lineNumber = 1;

        while (std::getline(in, line))
        {
            ++lineNumber;
            strip_trailing_cr(line);

            if (line.empty())
                continue;

            const std::vector<std::string> fields = split_simple(line, delim);
            ++result.rowsChecked;

            if (fields.size() != result.expectedColumns)
            {
                result.ok = false;
                result.firstBadLine = lineNumber;
                result.actualColumns = fields.size();
                break;
            }

            for (std::size_t i = 0; i < fields.size(); ++i)
                update_profile(result.profiles[i], fields[i]);
        }

        return result;
    }

    void run_preview_file_profile(const std::string& filePath, char delim)
    {
        const ValidateResult result = run_profile_scan(filePath, delim);
        if (result.headers.empty())
            return;

        std::cout << "Delimiter: " << delimiter_label(delim) << "\n";
        print_columns_report(result.headers, result.normalizedHeaders, &result.profiles);
        print_header_warnings(result.headerIssue);

        std::cout << "\n";
        std::cout << "IMPORTSQL preview profiling hook installed successfully\n";
    }

    void print_validate_report(const ValidateResult& result, char delim)
    {
        std::cout << "Delimiter: " << delimiter_label(delim) << "\n";
        std::cout << "\n";
        std::cout << "Validation Report\n";
        std::cout << "  Header columns: " << result.headerColumnCount << "\n";
        std::cout << "  Data rows checked: " << result.rowsChecked << "\n";

        print_columns_report(result.headers, result.normalizedHeaders, &result.profiles);
        print_header_warnings(result.headerIssue);

        if (result.firstBadLine != 0)
        {
            std::cout << "\n";
            std::cout << "Errors\n";
            std::cout << "  Line " << result.firstBadLine
                      << ": expected " << result.expectedColumns
                      << " column(s), found " << result.actualColumns << "\n";
        }

        std::cout << "\n";
        if (result.ok)
            std::cout << "VALIDATION: OK\n";
        else
            std::cout << "VALIDATION: FAILED\n";
    }

    char parse_common_options(std::istringstream& iss)
    {
        char delim = '|';

        std::string kw;
        while (iss >> kw)
        {
            const std::string upperKw = normalize_field_name(kw);
            if (upperKw == "DELIM")
            {
                std::string delimName;
                iss >> delimName;
                if (!delimName.empty())
                    delim = detect_delimiter_from_name(delimName);
            }
        }

        return delim;
    }

    bool normalize_date_for_dbf(const std::string& input, std::string& output)
    {
        if (input.empty())
        {
            output.clear();
            return true;
        }

        if (!dottalkpp::import::is_date_text(input))
            return false;

        output = input.substr(0, 4) + input.substr(5, 2) + input.substr(8, 2);
        return true;
    }

    bool normalize_logical_for_dbf(const std::string& input, std::string& output)
    {
        if (input.empty())
        {
            output.clear();
            return true;
        }

        const std::string u = normalize_field_name(input);

        if (u == "T" || u == "TRUE" || u == "Y" || u == "YES" || u == "1")
        {
            output = "T";
            return true;
        }

        if (u == "F" || u == "FALSE" || u == "N" || u == "NO" || u == "0")
        {
            output = "F";
            return true;
        }

        return false;
    }

    bool convert_field_value(const std::string& input,
                             const ColumnProfile& profile,
                             std::string& output,
                             std::string& error)
    {
        const char targetType = proposed_target_type(profile);
        const int targetWidth = proposed_target_width(profile);
        const int targetDec = proposed_target_decimals(profile);

        (void)targetDec;

        if (targetType == 'C')
        {
            if (static_cast<int>(input.size()) > targetWidth)
            {
                error = "character width overflow";
                return false;
            }

            output = input;
            return true;
        }

        if (targetType == 'N')
        {
            if (input.empty())
            {
                output.clear();
                return true;
            }

            if (!(dottalkpp::import::is_integer_text(input) ||
                  dottalkpp::import::is_decimal_text(input)))
            {
                error = "invalid numeric value";
                return false;
            }

            if (static_cast<int>(input.size()) > targetWidth)
            {
                error = "numeric width overflow";
                return false;
            }

            output = input;
            return true;
        }

        if (targetType == 'D')
        {
            if (!normalize_date_for_dbf(input, output))
            {
                error = "invalid date value";
                return false;
            }
            return true;
        }

        if (targetType == 'L')
        {
            if (!normalize_logical_for_dbf(input, output))
            {
                error = "invalid logical value";
                return false;
            }
            return true;
        }

        if (targetType == 'M')
        {
            output = input;
            return true;
        }

        error = "unsupported target type";
        return false;
    }

    bool current_area_matches_target(xbase::DbArea& area, const std::string& table)
    {
        if (!area.isOpen())
            return false;

        const std::string openStem =
            normalize_field_name(std::filesystem::path(area.filename()).stem().string());

        const std::string targetStem = normalize_field_name(table);

        return openStem == targetStem;
    }

    void run_file_import(xbase::DbArea& area,
                         const std::string& filePath,
                         const std::string& tableName,
                         char delim)
    {
        if (filePath.empty())
        {
            std::cout << "IMPORTSQL FILE requires a source file\n";
            return;
        }

        if (tableName.empty())
        {
            std::cout << "IMPORTSQL FILE requires a target table name\n";
            return;
        }

        if (!current_area_matches_target(area, tableName))
        {
            std::cout << "IMPORTSQL FILE requires target table " << tableName
                      << " to be open in the current area\n";
            std::cout << "Open it first with USE " << tableName << "\n";
            return;
        }

        const ValidateResult scan = run_profile_scan(filePath, delim);

        if (scan.headers.empty())
            return;

        print_header_warnings(scan.headerIssue);

        if (scan.firstBadLine != 0)
        {
            std::cout << "\nErrors\n";
            std::cout << "  Line " << scan.firstBadLine
                      << ": expected " << scan.expectedColumns
                      << " column(s), found " << scan.actualColumns << "\n";
            std::cout << "\nIMPORT: FAILED\n";
            return;
        }

        if (!scan.ok)
        {
            std::cout << "\nIMPORT: FAILED\n";
            return;
        }

        std::ifstream in;
        if (!open_text_file(filePath, in))
        {
            std::cout << "Unable to open file: " << filePath << "\n";
            return;
        }

        std::string line;
        if (!std::getline(in, line))
        {
            std::cout << "File is empty: " << filePath << "\n";
            return;
        }

        std::size_t lineNumber = 1;
        std::size_t importedRows = 0;

        while (std::getline(in, line))
        {
            ++lineNumber;
            strip_trailing_cr(line);

            if (line.empty())
                continue;

            const std::vector<std::string> fields = split_simple(line, delim);

            if (fields.size() != scan.expectedColumns)
            {
                std::cout << "IMPORT: FAILED\n";
                std::cout << "  Line " << lineNumber
                          << ": expected " << scan.expectedColumns
                          << " column(s), found " << fields.size() << "\n";
                return;
            }

            std::vector<std::string> converted(fields.size());

            for (std::size_t i = 0; i < fields.size(); ++i)
            {
                std::string error;
                if (!convert_field_value(fields[i], scan.profiles[i], converted[i], error))
                {
                    std::cout << "IMPORT: FAILED\n";
                    std::cout << "  Line " << lineNumber
                              << ", column " << (i + 1)
                              << " (" << scan.normalizedHeaders[i] << "): "
                              << error << "\n";
                    return;
                }
            }

            if (!area.appendBlank())
            {
                std::cout << "IMPORT: FAILED\n";
                std::cout << "  Line " << lineNumber
                          << ": appendBlank() failed\n";
                return;
            }

            for (std::size_t i = 0; i < converted.size(); ++i)
            {
                if (!area.set(static_cast<int>(i + 1), converted[i]))
                {
                    std::cout << "IMPORT: FAILED\n";
                    std::cout << "  Line " << lineNumber
                              << ", column " << (i + 1)
                              << " (" << scan.normalizedHeaders[i] << "): set() failed\n";
                    return;
                }
            }

            if (!area.writeCurrent())
            {
                std::cout << "IMPORT: FAILED\n";
                std::cout << "  Line " << lineNumber
                          << ": writeCurrent() failed\n";
                return;
            }

            ++importedRows;
        }

        std::cout << "\n";
        std::cout << "IMPORT: OK\n";
        std::cout << "  Rows imported: " << importedRows << "\n";
    }
}

void cmd_IMPORTSQL(xbase::DbArea& area, std::istringstream& iss)
{
    (void)area;

    std::cout << "IMPORTSQL command invoked\n";

    std::string subcmd;
    iss >> subcmd;

    if (subcmd.empty())
    {
        std::cout << "Usage:\n";
        std::cout << "  IMPORTSQL PREVIEW <file> [DELIM PIPE|TAB|COMMA]\n";
        std::cout << "  IMPORTSQL VALIDATE <file> [DELIM PIPE|TAB|COMMA]\n";
        std::cout << "  IMPORTSQL SCHEMA <file> [DELIM PIPE|TAB|COMMA]\n";
        std::cout << "  IMPORTSQL CREATE <file> TO <table> [DELIM PIPE|TAB|COMMA]\n";
        std::cout << "  IMPORTSQL FILE <file> TO <table> [DELIM PIPE|TAB|COMMA]\n";
        std::cout << "  IMPORTSQL MAP <subcommand> <mapfile>\n";
        return;
    }

    std::string mode = normalize_field_name(subcmd);

    if (mode == "IMPORTSQL")
    {
        iss >> subcmd;
        if (subcmd.empty())
        {
            std::cout << "Usage:\n";
            std::cout << "  IMPORTSQL PREVIEW <file> [DELIM PIPE|TAB|COMMA]\n";
            std::cout << "  IMPORTSQL VALIDATE <file> [DELIM PIPE|TAB|COMMA]\n";
            std::cout << "  IMPORTSQL SCHEMA <file> [DELIM PIPE|TAB|COMMA]\n";
            std::cout << "  IMPORTSQL CREATE <file> TO <table> [DELIM PIPE|TAB|COMMA]\n";
            std::cout << "  IMPORTSQL FILE <file> TO <table> [DELIM PIPE|TAB|COMMA]\n";
            std::cout << "  IMPORTSQL MAP <subcommand> <mapfile>\n";
            return;
        }
        mode = normalize_field_name(subcmd);
    }

    if (mode == "PREVIEW")
    {
        std::string file;
        iss >> file;
        const char delim = parse_common_options(iss);

        std::cout << "Mode: PREVIEW\n";
        if (!file.empty())
            std::cout << "File: " << file << "\n";

        run_preview_file_profile(file, delim);
    }
    else if (mode == "VALIDATE")
    {
        std::string file;
        iss >> file;
        const char delim = parse_common_options(iss);

        std::cout << "Mode: VALIDATE\n";
        if (!file.empty())
            std::cout << "File: " << file << "\n";

        const ValidateResult result = run_profile_scan(file, delim);
        print_validate_report(result, delim);
    }
    else if (mode == "SCHEMA")
    {
        std::string file;
        iss >> file;
        const char delim = parse_common_options(iss);

        std::cout << "Mode: SCHEMA\n";
        if (!file.empty())
            std::cout << "File: " << file << "\n";

        const ValidateResult result = run_profile_scan(file, delim);
        std::cout << "Delimiter: " << delimiter_label(delim) << "\n";
        print_header_warnings(result.headerIssue);

        if (result.firstBadLine != 0)
        {
            std::cout << "\nErrors\n";
            std::cout << "  Line " << result.firstBadLine
                      << ": expected " << result.expectedColumns
                      << " column(s), found " << result.actualColumns << "\n";
            std::cout << "\nSCHEMA: FAILED\n";
            return;
        }

        if (!result.ok)
        {
            std::cout << "\nSCHEMA: FAILED\n";
            return;
        }

        print_schema_report(result);

        std::cout << "\nSCHEMA: OK\n";
    }
    else if (mode == "CREATE")
    {
        std::string file;
        std::string toKw;
        std::string table;
        iss >> file >> toKw >> table;

        const char delim = parse_common_options(iss);

        std::cout << "Mode: CREATE\n";
        if (!file.empty())
            std::cout << "File: " << file << "\n";
        if (!table.empty())
            std::cout << "Target: " << table << "\n";
        std::cout << "Delimiter: " << delimiter_label(delim) << "\n";

        if (normalize_field_name(toKw) != "TO")
        {
            std::cout << "\nCREATE: FAILED\n";
            std::cout << "  Expected syntax: IMPORTSQL CREATE <file> TO <table>\n";
            return;
        }

        const ValidateResult result = run_profile_scan(file, delim);

        print_header_warnings(result.headerIssue);

        if (result.firstBadLine != 0)
        {
            std::cout << "\nErrors\n";
            std::cout << "  Line " << result.firstBadLine
                      << ": expected " << result.expectedColumns
                      << " column(s), found " << result.actualColumns << "\n";
            std::cout << "\nCREATE: FAILED\n";
            return;
        }

        if (!result.ok)
        {
            std::cout << "\nCREATE: FAILED\n";
            return;
        }

        print_create_report(result, table);

        std::cout << "\nCREATE: OK\n";
    }
    else if (mode == "FILE")
    {
        std::string file;
        std::string toKw;
        std::string table;
        iss >> file >> toKw >> table;

        const char delim = parse_common_options(iss);

        std::cout << "Mode: FILE\n";
        if (!file.empty())
            std::cout << "File: " << file << "\n";
        if (!table.empty())
            std::cout << "Target: " << table << "\n";
        std::cout << "Delimiter: " << delimiter_label(delim) << "\n";

        if (normalize_field_name(toKw) != "TO")
        {
            std::cout << "\nIMPORT: FAILED\n";
            std::cout << "  Expected syntax: IMPORTSQL FILE <file> TO <table>\n";
            return;
        }

        run_file_import(area, file, table, delim);
    }
    else if (mode == "MAP")
    {
        std::string sub;
        std::string mapfile;

        iss >> sub >> mapfile;

        std::cout << "Mode: MAP\n";
        if (!sub.empty())
            std::cout << "Map subcommand: " << sub << "\n";
        if (!mapfile.empty())
            std::cout << "Map file: " << mapfile << "\n";

        std::cout << "MAP hook installed successfully (implementation pending)\n";
    }
    else
    {
        std::cout << "Unknown IMPORTSQL subcommand: " << subcmd << "\n";
    }
}

void cmd_EXPORTSQL(xbase::DbArea& area, std::istringstream& iss)
{
    (void)area;

    std::cout << "EXPORTSQL command invoked\n";

    std::string subcmd;
    iss >> subcmd;

    if (subcmd.empty())
    {
        std::cout << "Usage:\n";
        std::cout << "  EXPORTSQL PREVIEW <table>\n";
        std::cout << "  EXPORTSQL FILE <table> TO <file>\n";
        return;
    }

    std::string mode = normalize_field_name(subcmd);

    if (mode == "EXPORTSQL")
    {
        iss >> subcmd;
        if (subcmd.empty())
        {
            std::cout << "Usage:\n";
            std::cout << "  EXPORTSQL PREVIEW <table>\n";
            std::cout << "  EXPORTSQL FILE <table> TO <file>\n";
            return;
        }
        mode = normalize_field_name(subcmd);
    }

    if (mode == "PREVIEW")
    {
        std::string table;
        iss >> table;

        std::cout << "Mode: PREVIEW\n";
        if (!table.empty())
            std::cout << "Table: " << table << "\n";

        std::cout << "EXPORTSQL PREVIEW hook installed successfully (implementation pending)\n";
    }
    else if (mode == "FILE")
    {
        std::string table;
        std::string to_kw;
        std::string file;

        iss >> table >> to_kw >> file;

        std::cout << "Mode: FILE\n";
        if (!table.empty())
            std::cout << "Table: " << table << "\n";
        if (!to_kw.empty())
            std::cout << "Keyword: " << to_kw << "\n";
        if (!file.empty())
            std::cout << "File: " << file << "\n";

        std::cout << "EXPORTSQL FILE hook installed successfully (implementation pending)\n";
    }
    else
    {
        std::cout << "Unknown EXPORTSQL subcommand: " << subcmd << "\n";
    }
}
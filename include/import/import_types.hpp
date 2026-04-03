#pragma once

#include <string>

namespace dottalkpp::import
{

    enum class Delimiter
    {
        Pipe,
        Tab,
        Comma,
        Custom
    };

    enum class Mode
    {
        Preview,
        Validate,
        File,
        Map
    };

    enum class Severity
    {
        Info,
        Warn,
        Error,
        Fatal
    };

    struct ImportOptions
    {
        std::string filePath;
        std::string mapFile;
        std::string targetTable;

        Delimiter delimiter = Delimiter::Pipe;

        bool strictMode = true;
        bool create = false;
        bool append = false;
        bool replace = false;

        std::string encoding = "UTF8";
        std::string nullToken = "\\N";

        int previewRows = 10;
    };

}

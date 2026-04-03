#include "dt/data/format_registry.hpp"

namespace dt::data {

std::vector<FormatInfo> supported_formats()
{
    return {
        { FormatKind::DBF,   "DBF",   {".dbf"},            true,  true  },
        { FormatKind::CSV,   "CSV",   {".csv"},            true,  true  },
        { FormatKind::FIXED, "FIXED", {".dat", ".txt"},    true,  true  },
        { FormatKind::TSV,   "TSV",   {".tsv"},            true,  true  },
        { FormatKind::JSON,  "JSON",  {".json"},           false, false },
        { FormatKind::XML,   "XML",   {".xml"},            false, false }
    };
}

} // namespace dt::data
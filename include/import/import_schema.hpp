#pragma once

#include <string>

namespace dottalkpp::import
{

    struct ColumnProfile
    {
        std::string sourceName;
        std::string normalizedName;

        int maxWidth = 0;

        bool seenInteger = false;
        bool seenDecimal = false;
        bool seenDate = false;
        bool seenLogical = false;
        bool seenText = false;

        int decimalPlaces = 0;
        bool hasNull = false;
    };


    struct FieldDefinition
    {
        std::string name;

        char type = 'C';
        int width = 0;
        int decimals = 0;
    };

}

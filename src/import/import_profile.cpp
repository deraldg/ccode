#include "import/import_profile.hpp"

#include <cctype>
#include <string>

#include "import/import_normalize.hpp"

namespace dottalkpp::import
{
    namespace
    {
        void inspect_numeric_shape(const std::string& s,
                                   std::size_t& integerDigits,
                                   std::size_t& decimalDigits,
                                   bool& negative)
        {
            integerDigits = 0;
            decimalDigits = 0;
            negative = false;

            if (s.empty())
                return;

            std::size_t i = 0;
            if (s[0] == '+' || s[0] == '-')
            {
                negative = (s[0] == '-');
                i = 1;
            }

            bool afterDot = false;

            for (; i < s.size(); ++i)
            {
                const char ch = s[i];

                if (ch == '.')
                {
                    afterDot = true;
                    continue;
                }

                if (!std::isdigit(static_cast<unsigned char>(ch)))
                    return;

                if (afterDot)
                    ++decimalDigits;
                else
                    ++integerDigits;
            }
        }
    }

    bool is_integer_text(const std::string& s)
    {
        if (s.empty())
            return false;

        std::size_t i = 0;
        if (s[0] == '+' || s[0] == '-')
        {
            if (s.size() == 1)
                return false;
            i = 1;
        }

        for (; i < s.size(); ++i)
        {
            if (!std::isdigit(static_cast<unsigned char>(s[i])))
                return false;
        }

        return true;
    }

    bool is_decimal_text(const std::string& s)
    {
        if (s.empty())
            return false;

        std::size_t i = 0;
        bool sawDot = false;
        bool sawDigit = false;

        if (s[0] == '+' || s[0] == '-')
        {
            if (s.size() == 1)
                return false;
            i = 1;
        }

        for (; i < s.size(); ++i)
        {
            const unsigned char ch = static_cast<unsigned char>(s[i]);

            if (std::isdigit(ch))
            {
                sawDigit = true;
                continue;
            }

            if (s[i] == '.')
            {
                if (sawDot)
                    return false;
                sawDot = true;
                continue;
            }

            return false;
        }

        return sawDigit && sawDot;
    }

    bool is_date_text(const std::string& s)
    {
        if (s.size() != 10)
            return false;

        return std::isdigit(static_cast<unsigned char>(s[0])) &&
               std::isdigit(static_cast<unsigned char>(s[1])) &&
               std::isdigit(static_cast<unsigned char>(s[2])) &&
               std::isdigit(static_cast<unsigned char>(s[3])) &&
               s[4] == '-' &&
               std::isdigit(static_cast<unsigned char>(s[5])) &&
               std::isdigit(static_cast<unsigned char>(s[6])) &&
               s[7] == '-' &&
               std::isdigit(static_cast<unsigned char>(s[8])) &&
               std::isdigit(static_cast<unsigned char>(s[9]));
    }

    bool is_logical_text(const std::string& s)
    {
        const std::string u = normalize_field_name(s);

        return u == "T"     || u == "F"     ||
               u == "TRUE"  || u == "FALSE" ||
               u == "Y"     || u == "N"     ||
               u == "YES"   || u == "NO"    ||
               u == "1"     || u == "0";
    }

    void update_profile(ColumnProfile& profile, const std::string& value)
    {
        if (value.empty())
            return;

        profile.sawAnyNonEmpty = true;

        if (value.size() > profile.maxLen)
            profile.maxLen = value.size();

        if (is_integer_text(value))
        {
            profile.seenInteger = true;

            std::size_t intDigits = 0;
            std::size_t decDigits = 0;
            bool negative = false;
            inspect_numeric_shape(value, intDigits, decDigits, negative);

            if (intDigits > profile.maxIntegerDigits)
                profile.maxIntegerDigits = intDigits;
            if (negative)
                profile.sawNegative = true;

            return;
        }

        if (is_decimal_text(value))
        {
            profile.seenDecimal = true;

            std::size_t intDigits = 0;
            std::size_t decDigits = 0;
            bool negative = false;
            inspect_numeric_shape(value, intDigits, decDigits, negative);

            if (intDigits > profile.maxIntegerDigits)
                profile.maxIntegerDigits = intDigits;
            if (decDigits > profile.maxDecimalDigits)
                profile.maxDecimalDigits = decDigits;
            if (negative)
                profile.sawNegative = true;

            return;
        }

        if (is_date_text(value))
        {
            profile.seenDate = true;
            return;
        }

        if (is_logical_text(value))
        {
            profile.seenLogical = true;
            return;
        }

        profile.seenText = true;
    }

    std::string classify_profile(const ColumnProfile& p)
    {
        int categories = 0;
        categories += p.seenInteger ? 1 : 0;
        categories += p.seenDecimal ? 1 : 0;
        categories += p.seenDate ? 1 : 0;
        categories += p.seenLogical ? 1 : 0;
        categories += p.seenText ? 1 : 0;

        if (!p.sawAnyNonEmpty)
            return "EMPTY";

        if (categories > 1)
        {
            // Integer + decimal is still decimal.
            if (p.seenDecimal &&
                p.seenInteger &&
                !p.seenDate &&
                !p.seenLogical &&
                !p.seenText)
            {
                return "DECIMAL";
            }

            return "MIXED";
        }

        if (p.seenInteger)
            return "INTEGER";
        if (p.seenDecimal)
            return "DECIMAL";
        if (p.seenDate)
            return "DATE";
        if (p.seenLogical)
            return "LOGICAL";
        if (p.seenText)
        {
            if (p.maxLen > 254)
                return "LONGTEXT";
            return "TEXT";
        }

        return "EMPTY";
    }

    char proposed_target_type(const ColumnProfile& p)
    {
        const std::string klass = classify_profile(p);

        if (klass == "INTEGER")
            return 'N';
        if (klass == "DECIMAL")
            return 'N';
        if (klass == "DATE")
            return 'D';
        if (klass == "LOGICAL")
            return 'L';
        if (klass == "LONGTEXT")
            return 'M';
        if (klass == "TEXT")
            return 'C';
        if (klass == "MIXED")
            return 'C';
        if (klass == "EMPTY")
            return 'C';

        return 'C';
    }

    int proposed_target_width(const ColumnProfile& p)
    {
        const std::string klass = classify_profile(p);

        if (klass == "INTEGER")
        {
            int width = static_cast<int>(p.maxIntegerDigits);
            if (p.sawNegative)
                ++width;
            if (width < 1)
                width = 1;
            return width;
        }

        if (klass == "DECIMAL")
        {
            int width = static_cast<int>(p.maxIntegerDigits);

            if (p.maxDecimalDigits > 0)
            {
                width += 1; // decimal point
                width += static_cast<int>(p.maxDecimalDigits);
            }

            if (p.sawNegative)
                ++width;

            if (width < 1)
                width = 1;

            return width;
        }

        if (klass == "DATE")
            return 8;

        if (klass == "LOGICAL")
            return 1;

        // Placeholder only; M does not use inline char width.
        if (klass == "LONGTEXT")
            return 10;

        if (klass == "TEXT" || klass == "MIXED" || klass == "EMPTY")
        {
            int width = static_cast<int>(p.maxLen);

            if (klass == "EMPTY" && width == 0)
                width = 1;

            // Cushion rule for character-like fields.
            if (klass == "TEXT" || klass == "MIXED")
                width += 2;

            if (width > 254)
                width = 254;

            if (width < 1)
                width = 1;

            return width;
        }

        return 1;
    }

    int proposed_target_decimals(const ColumnProfile& p)
    {
        const std::string klass = classify_profile(p);

        if (klass == "DECIMAL")
            return static_cast<int>(p.maxDecimalDigits);

        return 0;
    }
}
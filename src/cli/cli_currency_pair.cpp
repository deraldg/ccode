#include "cli_currency_pair.hpp"

#include "xbase.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace cli_currency {
namespace {

static std::string trim_copy(std::string s) {
    std::size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;

    std::size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;

    return s.substr(a, b - a);
}

static std::string up_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

static bool ends_with_ci(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) return false;

    const std::size_t off = s.size() - suffix.size();
    for (std::size_t i = 0; i < suffix.size(); ++i) {
        const unsigned char a = static_cast<unsigned char>(s[off + i]);
        const unsigned char b = static_cast<unsigned char>(suffix[i]);
        if (std::toupper(a) != std::toupper(b)) return false;
    }
    return true;
}

static int resolve_field_index_by_name_ci(xbase::DbArea& A, const std::string& name) {
    try {
        const std::string want = up_copy(name);
        const auto defs = A.fields();
        for (std::size_t i = 0; i < defs.size(); ++i) {
            if (up_copy(defs[i].name) == want) return static_cast<int>(i) + 1;
        }
    } catch (...) {
    }
    return 0;
}

static char field_type_upper(xbase::DbArea& A, int field1) {
    try {
        const auto defs = A.fields();
        const int idx0 = field1 - 1;
        if (idx0 < 0 || idx0 >= static_cast<int>(defs.size())) return '\0';
        return static_cast<char>(std::toupper(static_cast<unsigned char>(defs[static_cast<std::size_t>(idx0)].type)));
    } catch (...) {
        return '\0';
    }
}

static bool normalize_currency_code(const std::string& raw_value, std::string& normalized) {
    std::string s = trim_copy(raw_value);
    if (s.size() != 3) return false;

    for (char& c : s) {
        if (!std::isalpha(static_cast<unsigned char>(c))) return false;
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    normalized = s;
    return true;
}

} // namespace

bool is_currency_pair_field_name(const std::string& field_name) {
    const std::string s = trim_copy(field_name);
    return ends_with_ci(s, "_CUR") || ends_with_ci(s, "_CU");
}

std::string currency_pair_base_name(const std::string& field_name) {
    std::string s = trim_copy(field_name);
    if (ends_with_ci(s, "_CUR")) return s.substr(0, s.size() - 4);
    if (ends_with_ci(s, "_CU"))  return s.substr(0, s.size() - 3);
    return s;
}

bool validate_and_normalize_currency_pair_field(
    xbase::DbArea& area,
    int field1,
    const std::string& raw_value,
    std::string& normalized_value,
    std::string& err)
{
    err.clear();
    normalized_value = raw_value;

    const auto defs = area.fields();
    const int idx0 = field1 - 1;
    if (idx0 < 0 || idx0 >= static_cast<int>(defs.size())) {
        err = "invalid field";
        return false;
    }

    const std::string field_name = defs[static_cast<std::size_t>(idx0)].name;
    if (!is_currency_pair_field_name(field_name)) {
        normalized_value = raw_value;
        return true;
    }

    const std::string base_name = currency_pair_base_name(field_name);
    const int base1 = resolve_field_index_by_name_ci(area, base_name);
    if (base1 <= 0) {
        err = "currency field has no paired amount field";
        return false;
    }

    if (field_type_upper(area, base1) != 'Y') {
        err = "currency field pair is not a Y field";
        return false;
    }

    if (!normalize_currency_code(raw_value, normalized_value)) {
        err = "invalid currency code";
        return false;
    }

    return true;
}

} // namespace cli_currency
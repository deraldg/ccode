#include "cli/memo_display.hpp"

#include "xbase.hpp"
#include "xbase_64.hpp"
#include "memo/memo_auto.hpp"
#include "memo/memostore.hpp"

#include <cstdint>
#include <string>

namespace cli_memo
{
namespace
{

static bool is_x64_memo_field(const xbase::DbArea& area, int field1)
{
    if (field1 < 1 || field1 > area.fieldCount()) return false;
    if (area.versionByte() != xbase::DBF_VERSION_64) return false;

    const auto& f = area.fields()[static_cast<std::size_t>(field1 - 1)];
    return (f.type == 'M' || f.type == 'm') &&
           f.length == xbase::X64_MEMO_FIELD_LEN;
}

static std::uint64_t parse_u64_or_zero(const std::string& s)
{
    if (s.empty()) return 0;
    try {
        std::size_t used = 0;
        const unsigned long long v = std::stoull(s, &used, 10);
        if (used != s.size()) return 0;
        return static_cast<std::uint64_t>(v);
    } catch (...) {
        return 0;
    }
}

static dottalk::memo::MemoStore* memo_store_for_area(xbase::DbArea& area) noexcept
{
    auto* backend = cli_memo::memo_backend_for(area);
    if (!backend) return nullptr;
    return dynamic_cast<dottalk::memo::MemoStore*>(backend);
}

} // namespace

std::string resolve_display_value(xbase::DbArea& area,
                                  int field1,
                                  const std::string& raw_value)
{
    if (!is_x64_memo_field(area, field1))
        return raw_value;

    const std::uint64_t object_id = parse_u64_or_zero(raw_value);
    if (object_id == 0)
        return std::string{};

    auto* store = memo_store_for_area(area);
    if (!store)
        return raw_value;

    std::string text;
    if (!store->get_text_id(object_id, text, nullptr))
        return raw_value;

    return text;
}

} // namespace cli_memo
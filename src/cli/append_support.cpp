// src/cli/append_support.cpp
//
// Shared APPEND / APPEND BLANK implementation.
// Generates numeric unique keys (SID fallback) and updates simple indexes.
//
// Raw append policy:
//   - RAW append still populates autokey / unique fields.
//   - RAW append does NOT update attached indexes inline.
//   - Rebuild is expected after RAW MANY bulk operations.

#include "cli/append_support.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "xbase.hpp"
#include "xbase_locks.hpp"
#include "cli/settings.hpp"
#include "cli/table_state.hpp"
#include "cli/unique_registry.hpp"
#include "xindex/index_manager.hpp"

extern "C" xbase::XBaseEngine* shell_engine(void);

using cli::Settings;

namespace {

static std::string up_copy(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

static std::string trim_copy(std::string s)
{
    auto is_space = [](unsigned char ch){ return std::isspace(ch) != 0; };

    while (!s.empty() && is_space(static_cast<unsigned char>(s.front())))
        s.erase(s.begin());

    while (!s.empty() && is_space(static_cast<unsigned char>(s.back())))
        s.pop_back();

    return s;
}

static int field_index_by_name_ci(xbase::DbArea& A, const std::string& name)
{
    const std::string want = up_copy(name);
    const auto defs = A.fields();

    for (size_t i = 0; i < defs.size(); ++i) {
        if (up_copy(defs[i].name) == want)
            return static_cast<int>(i) + 1;
    }

    return 0;
}

static char field_type_upper(xbase::DbArea& A, int field1)
{
    try {
        if (field1 < 1 || field1 > A.fieldCount()) return '\0';
        return static_cast<char>(
            std::toupper(static_cast<unsigned char>(
                A.fields()[static_cast<std::size_t>(field1 - 1)].type)));
    } catch (...) {
        return '\0';
    }
}

static long long compute_next_numeric(xbase::DbArea& A, int field1)
{
    long long mx = std::numeric_limits<long long>::min();

    const int32_t save = A.recno();
    const int32_t total = A.recCount();

    for (int32_t r = 1; r <= total; ++r)
    {
        if (!A.gotoRec(r)) continue;
        if (!A.readCurrent()) continue;
        if (A.isDeleted()) continue;

        std::string v = A.get(field1);
        if (v.empty()) continue;

        try
        {
            long long n = std::stoll(v);
            if (n > mx) mx = n;
        }
        catch (...)
        {
        }
    }

    if (save > 0) {
        A.gotoRec(save);
        A.readCurrent();
    }

    if (mx == std::numeric_limits<long long>::min())
        return 1;

    return mx + 1;
}

static void generate_sid_if_needed(xbase::DbArea& A, bool& wrote)
{
    const int sid = field_index_by_name_ci(A, "SID");
    if (sid <= 0) return;

    const std::string v = A.get(sid);
    if (!trim_copy(v).empty()) return;

    const long long next = compute_next_numeric(A, sid);
    A.set(sid, std::to_string(next));
    wrote = true;
}

static void generate_registered_uniques(xbase::DbArea& A, bool& wrote)
{
    std::vector<std::string> uniq;

    try
    {
        uniq = unique_reg::list_unique_fields(A);
    }
    catch (...)
    {
        return;
    }

    for (const auto& f : uniq)
    {
        const int idx = field_index_by_name_ci(A, f);
        if (idx <= 0) continue;

        const std::string v = A.get(idx);
        if (!trim_copy(v).empty()) continue;

        const long long next = compute_next_numeric(A, idx);
        A.set(idx, std::to_string(next));
        wrote = true;
    }
}

static bool is_meaningful_index_value(xbase::DbArea& A, int field1, const std::string& raw)
{
    const std::string v = trim_copy(raw);
    const char t = field_type_upper(A, field1);

    switch (t)
    {
        case 'C':
        case 'M':
            return !v.empty();

        case 'D':
            return !v.empty();

        case 'L':
            return !v.empty();

        case 'N':
        case 'F':
        case 'I':
        case 'B':
        case 'Y':
            return !v.empty();

        default:
            return !v.empty();
    }
}

static void append_simple_meaningful_index_entries(xbase::DbArea& A, std::uint32_t rn)
{
    auto& im = A.indexManager();
    const auto defs = A.fields();

    for (std::size_t i = 0; i < defs.size(); ++i)
    {
        const int field1 = static_cast<int>(i) + 1;

        std::string val;
        try {
            val = A.get(field1);
        } catch (...) {
            continue;
        }

        if (!is_meaningful_index_value(A, field1, val))
            continue;

        try
        {
            (void)im.append_active_field_value(field1, val, static_cast<xindex::RecNo>(rn));
        }
        catch (...)
        {
            // Skip tags that do not exist or cannot be maintained in this simple phase.
        }
    }
}

// Shared post-append record initialization.
// This keeps autokey generation in APPEND, not in rebuild.
static bool finalize_appended_record(xbase::DbArea& A, bool update_index_inline, std::uint32_t rn)
{
    bool wrote = false;

    try
    {
        if (!A.readCurrent())
            return false;

        generate_registered_uniques(A, wrote);
        generate_sid_if_needed(A, wrote);

        if (wrote) {
            if (!A.writeCurrent())
                return false;
        }

        if (update_index_inline) {
            if (!A.readCurrent())
                return false;

            append_simple_meaningful_index_entries(A, rn);
        }

        return true;
    }
    catch (...)
    {
        return false;
    }
}

} // namespace

bool dottalk_append_many_core(xbase::DbArea& A, std::size_t count)
{
    if (!A.isOpen())
    {
        std::cout << "APPEND: no file open\n";
        return false;
    }

    if (count == 0)
        return true;

    std::string err;
    if (!xbase::locks::try_lock_table(A, &err))
    {
        std::cout << "APPEND: table locked (" << err << ")\n";
        return false;
    }

    bool ok = true;
    std::size_t done = 0;
    std::uint32_t rn = 0;

    for (; done < count; ++done)
    {
        try
        {
            if (!A.appendBlank())
            {
                ok = false;
                break;
            }

            rn = static_cast<std::uint32_t>(A.recno());

            if (!finalize_appended_record(A, /*update_index_inline=*/true, rn))
            {
                ok = false;
                break;
            }
        }
        catch (...)
        {
            ok = false;
            break;
        }
    }

    xbase::locks::unlock_table(A);

    if (!ok)
    {
        std::cout << "APPEND MANY: stopped after "
                  << done << " successful append(s)\n";
        return false;
    }

    if (Settings::instance().talk_on.load())
        std::cout << "Appended " << count << " blank record(s)\n";

    return true;
}

bool dottalk_append_blank_raw_locked(xbase::DbArea& A, std::uint32_t& rn)
{
    rn = 0;

    try
    {
        if (!A.appendBlank())
            return false;

        rn = static_cast<std::uint32_t>(A.recno());

        // RAW append still initializes autokey / unique fields.
        // It simply skips inline index maintenance.
        return finalize_appended_record(A, /*update_index_inline=*/false, rn);
    }
    catch (...)
    {
        rn = 0;
        return false;
    }
}

bool dottalk_append_blank_raw(xbase::DbArea& A, std::uint32_t& rn)
{
    rn = 0;

    if (!A.isOpen())
    {
        std::cout << "APPEND: no file open\n";
        return false;
    }

    std::string err;
    if (!xbase::locks::try_lock_table(A, &err))
    {
        std::cout << "APPEND: table locked (" << err << ")\n";
        return false;
    }

    const bool ok = dottalk_append_blank_raw_locked(A, rn);

    xbase::locks::unlock_table(A);

    if (!ok)
    {
        std::cout << "APPEND failed\n";
        return false;
    }

    return true;
}

bool dottalk_append_many_raw(xbase::DbArea& A, std::size_t count)
{
    if (!A.isOpen())
    {
        std::cout << "APPEND: no file open\n";
        return false;
    }

    if (count == 0)
        return true;

    std::string err;
    if (!xbase::locks::try_lock_table(A, &err))
    {
        std::cout << "APPEND: table locked (" << err << ")\n";
        return false;
    }

    bool ok = true;
    std::uint32_t rn = 0;
    std::size_t done = 0;

    for (; done < count; ++done)
    {
        if (!dottalk_append_blank_raw_locked(A, rn))
        {
            ok = false;
            break;
        }
    }

    xbase::locks::unlock_table(A);

    if (!ok)
    {
        std::cout << "APPEND RAW MANY: stopped after "
                  << done << " successful append(s)\n";
        return false;
    }

    if (Settings::instance().talk_on.load())
        std::cout << "Appended " << count << " raw blank record(s)\n";

    return true;
}

bool dottalk_append_blank_core(xbase::DbArea& A, std::istringstream&)
{
    if (!A.isOpen())
    {
        std::cout << "APPEND: no file open\n";
        return false;
    }

    std::string err;
    if (!xbase::locks::try_lock_table(A, &err))
    {
        std::cout << "APPEND: table locked (" << err << ")\n";
        return false;
    }

    bool ok = false;
    std::uint32_t rn = 0;

    try
    {
        if (A.appendBlank())
        {
            rn = static_cast<std::uint32_t>(A.recno());
            ok = finalize_appended_record(A, /*update_index_inline=*/true, rn);
        }
    }
    catch (...)
    {
        ok = false;
    }

    xbase::locks::unlock_table(A);

    if (!ok)
    {
        std::cout << "APPEND failed\n";
        return false;
    }

    if (Settings::instance().talk_on.load())
        std::cout << "Appended blank record " << A.recno() << "\n";

    return true;
}
#pragma once
#include <iosfwd>
#include <string>

#include "dt/data/rowset.hpp"
#include "dt/data/schema.hpp"
#include "dt/data/format_profile.hpp"

namespace dt::data {

bool write_rowset_as_fixed(
    std::ostream& out,
    const RowSet& rowset,
    const FixedProfile& profile,
    std::string* error = nullptr
);

RowSet read_rows_from_fixed(
    std::istream& in,
    const Schema& schema,
    const FixedProfile& profile,
    std::string* error = nullptr
);

} // namespace dt::data
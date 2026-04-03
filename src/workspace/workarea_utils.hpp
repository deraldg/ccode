#pragma once

#include <cstddef>
#include <string>

namespace workareas {

// Returns compressed occupied-slot description like:
//   {}
//   {5}
//   {0..16}
//   {0..19,50..54}
std::string occupied_desc();

// Returns count of open slots.
std::size_t open_count();

} // namespace workareas

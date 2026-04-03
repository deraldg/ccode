#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace xbase::dbf_create {

enum class Flavor {
    MSDOS,
    FOX26,
    VFP,
    X64
};

struct FieldSpec {
    std::string  name;
    char         type{};
    std::uint8_t len{};
    std::uint8_t dec{};
};

std::string flavor_name(Flavor flavor);

bool supports_type_now(char code, Flavor flavor) noexcept;

bool create_dbf(const std::string& path,
                const std::vector<FieldSpec>& fields,
                Flavor flavor,
                std::string& err);

} // namespace xbase::dbf_create

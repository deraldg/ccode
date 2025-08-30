// src/xindex/simple_index_build_and_save.cpp
#include <fstream>
#include <filesystem>
#include <string>

#include "xindex/simple_index.hpp"
#include "xbase.hpp"

namespace xindex {

// EXACT signature the linker is asking for:
// bool SimpleIndex::build_and_save(xbase::DbArea&,
//                                  const IndexMeta&,
//                                  const std::filesystem::path&,
//                                  std::string*)
// Minimal stub that just creates the output file with a tiny header.
// Replace later with the real builder.
bool SimpleIndex::build_and_save(xbase::DbArea& /*A*/,
                                 const IndexMeta& /*meta*/,
                                 const std::filesystem::path& outPath,
                                 std::string* err)
{
    try {
        std::error_code ec;
        auto parent = outPath.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent, ec);
        }

        std::ofstream f(outPath, std::ios::binary | std::ios::trunc);
        if (!f) {
            if (err) *err = "open failed: " + outPath.string();
            return false;
        }
        static const char kMagic[] = "XIDX0\n";
        f.write(kMagic, sizeof(kMagic) - 1);
        return static_cast<bool>(f);
    } catch (const std::exception& ex) {
        if (err) *err = ex.what();
        return false;
    }
}

} // namespace xindex

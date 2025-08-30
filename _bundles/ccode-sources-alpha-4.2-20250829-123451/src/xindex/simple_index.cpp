// src/xindex/simple_index.cpp  (add this if missing, or append to the file)
#include <filesystem>
#include <fstream>
#include <string>
#include "xindex/simple_index.hpp"   // declares xindex::SimpleIndex, IndexMeta
#include "xbase.hpp"

namespace xindex {

bool SimpleIndex::build_and_save(xbase::DbArea& /*A*/,
                                 const IndexMeta& /*meta*/,
                                 const std::filesystem::path& outPath,
                                 std::string* err)
{
    try {
        // Make sure the directory exists
        if (!outPath.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(outPath.parent_path(), ec);
        }

        // Touch/write a tiny header so the file isn't empty
        std::ofstream out(outPath, std::ios::binary);
        if (!out) {
            if (err) *err = std::string("cannot open index file: ") + outPath.string();
            return false;
        }
        static constexpr char kMagic[] = "DTI0"; // placeholder magic/version
        out.write(kMagic, sizeof(kMagic)-1);
        out.flush();
        return out.good();
    } catch (const std::exception& ex) {
        if (err) *err = ex.what();
        return false;
    }
}

} // namespace xindex

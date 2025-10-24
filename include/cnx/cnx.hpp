// include/cnx/cnx.hpp
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace cnx {

// One key entry in a tag (key bytes already encoded for sort, plus recno).
struct KeyEntry {
    std::vector<uint8_t> key;
    uint32_t             recno{};
};

// A tag in the CNX file (simple single “BASE” list for now).
struct Tag {
    std::vector<KeyEntry> base;
};

// Simple CNX container: multiple named tags stored in a single file.
// On-disk encoding is little-endian and starts with "CNX1".
class CNXFile {
public:
    explicit CNXFile(const std::string& path);
    ~CNXFile();

    // Create an empty CNX on disk, then return a handle bound to that path.
    static CNXFile CreateNew(const std::string& path);

    // Introspection
    std::vector<std::string> listTagNames() const;
    Tag* getTag(const std::string& name);

    // Edit
    bool dropTag(const std::string& name);
    void rebuildTag(
        const std::string& name,
        const std::function<void(std::function<void(KeyEntry)>)>& producer);
    void compactAll(); // rewrite from memory

private:
    void save_to_disk() const; // write all tags to path_

    std::string              path_;
    std::map<std::string,Tag> tags_;  // name -> Tag
};

} // namespace cnx

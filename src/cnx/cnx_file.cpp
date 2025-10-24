// src/cnx/cnx_file.cpp
#include "cnx/cnx.hpp"
#include <string>
#include <vector>
#include <functional>

namespace cnx {

CNXFile::CNXFile(const std::string& /*path*/) {}
CNXFile::~CNXFile() = default;

CNXFile CNXFile::CreateNew(const std::string& path) {
    return CNXFile(path);
}

std::vector<std::string> CNXFile::listTagNames() const {
    return {};
}

Tag* CNXFile::getTag(const std::string& /*name*/) {
    return nullptr;
}

bool CNXFile::dropTag(const std::string& /*name*/) {
    return false;
}

void CNXFile::rebuildTag(
    const std::string& /*name*/,
    const std::function<void(std::function<void(KeyEntry)>)>& /*producer*/
) {
    // no-op
}

void CNXFile::compactAll() {
    // no-op
}

} // namespace cnx

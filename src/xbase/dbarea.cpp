#include "xbase.hpp"
#include "xindex/index_manager.hpp"  // satisfies the forward-declared unique_ptr

#include <utility>

namespace xbase {

DbArea::DbArea() = default;
DbArea::~DbArea() = default;

// Single canonical definition for close(). Do NOT define this in dbf_file.cpp.
void DbArea::close() {
    if (_fp.is_open()) {
        _fp.close();
    }
    _filename.clear();
    _db_name.clear();
    _recbuf.clear();
    _fields.clear();
    _rawFields.clear();
    _fd.clear();
    _fd_snapshot.clear();
    _crn = 0;
    _del = NOT_DELETED;
}

std::string DbArea::filename() const {
    return _filename.empty() ? _db_name : _filename;
}

void DbArea::setFilename(std::string path) {
    _filename = std::move(path);
}

int DbArea::recordLength() const noexcept {
    return _hdr.cpr;
}

} // namespace xbase

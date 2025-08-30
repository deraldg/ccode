// memo_sidecar_v1/src/memo/memostore.cpp
//
// Skeleton implementation. Wire to your platform file I/O layer in the project.
// This file is intentionally minimal (no real I/O) to serve as a starting point.

#include "memo/memostore.hpp"
#include "memo/dtx_format.hpp"
#include <cstdio>
#include <cstring>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

namespace xbase::memo {

struct MemoStore::Impl {
    std::string basepath;   // without extension
    fs::path    dtx_path;   // "<basepath>.dtx"
    StoreStat   sstat{};

    Impl(std::string bp) : basepath(std::move(bp)) {
        dtx_path = fs::path(basepath).replace_extension(".dtx");
    }
};

MemoStore MemoStore::open(const std::string& basepath) {
    MemoStore m;
    m.impl_ = new Impl(basepath);
    // TODO: open file, read header, validate CRC/magic/version
    // For now, just check existence
    if (!fs::exists(m.impl_->dtx_path)) {
        throw io_error("Memo sidecar does not exist: " + m.impl_->dtx_path.string());
    }
    return std::move(m);
}

MemoStore MemoStore::create(const std::string& basepath, uint32_t block_size) {
    (void)block_size; // TODO: use for header init
    MemoStore m;
    m.impl_ = new Impl(basepath);
    // TODO: create/truncate file, write fresh header
    // Here we just touch the file.
    std::FILE* f = std::fopen(m.impl_->dtx_path.string().c_str(), "wb");
    if (!f) throw io_error("Failed to create: " + m.impl_->dtx_path.string());
    // Minimal header placeholder; a real impl must write DtxHeader fully.
    std::array<char, 4096> zero{};
    std::fwrite(zero.data(), 1, zero.size(), f);
    std::fclose(f);
    return std::move(m);
}

MemoStore::MemoStore() : impl_(nullptr) {}
MemoStore::MemoStore(MemoStore&& o) noexcept : impl_(o.impl_) { o.impl_ = nullptr; }
MemoStore& MemoStore::operator=(MemoStore&& o) noexcept { 
    if (this != &o) { delete impl_; impl_ = o.impl_; o.impl_ = nullptr; }
    return *this; 
}
MemoStore::~MemoStore() { delete impl_; }

uint64_t MemoStore::put(const std::vector<uint8_t>& data) {
    // TODO: append object, update id-map, stats; return id
    (void)data;
    throw memo_error("MemoStore::put not implemented");
}

std::vector<uint8_t> MemoStore::get(uint64_t object_id) const {
    (void)object_id;
    throw not_found("MemoStore::get not implemented");
}

void MemoStore::update(uint64_t object_id, const std::vector<uint8_t>& data) {
    (void)object_id; (void)data;
    throw memo_error("MemoStore::update not implemented");
}

void MemoStore::erase(uint64_t object_id) {
    (void)object_id;
    throw memo_error("MemoStore::erase not implemented");
}

MemoStat MemoStore::stat(uint64_t object_id) const {
    (void)object_id;
    MemoStat s; s.exists = false; return s;
}

void MemoStore::flush() { /* TODO: fsync */ }

StoreStat MemoStore::store_stats() const { return impl_ ? impl_->sstat : StoreStat{}; }

void MemoStore::memocheck(bool repair) { (void)repair; /* TODO: verify CRCs, rebuild id map */ }

std::string MemoStore::sidecar_path() const {
    return impl_ ? impl_->dtx_path.string() : std::string();
}

} // namespace xbase::memo

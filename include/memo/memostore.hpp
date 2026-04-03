// memostore.hpp
// DTX-backed memo store for DotTalk++
//
// This is the concrete DTX v1 backend declaration.
// It implements IMemoBackend while keeping the file-format details in dtx_format.hpp.

#pragma once

#include "memo_backend.hpp"
#include "dtx_format.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace dottalk::memo {

// DTX implementation.
// Text-first, append-mostly, simple lookup.
// Free-space reuse and advanced repair are deferred.
class MemoStore final : public IMemoBackend {
public:
    MemoStore() = default;
    ~MemoStore() override;

    MemoStore(const MemoStore&) = delete;
    MemoStore& operator=(const MemoStore&) = delete;

    MemoStore(MemoStore&&) noexcept;
    MemoStore& operator=(MemoStore&&) noexcept;

    // IMemoBackend
    BackendKind kind() const noexcept override { return BackendKind::DTX; }

    bool is_open() const noexcept override;
    std::string path() const override;

    MemoOpResult open(const std::string& sidecar_path, OpenMode mode) override;
    MemoOpResult flush() override;
    void close() noexcept override;

    MemoPutResult put_text(std::string_view text) override;
    MemoGetResult get_text(const MemoRef& ref) override;
    MemoPutResult update_text(const MemoRef& old_ref, std::string_view text) override;
    MemoOpResult  erase(const MemoRef& ref) override;
    MemoStat      stat(const MemoRef& ref) override;

    MemoRef make_null_ref() const override;
    bool is_null_ref(const MemoRef& ref) const override;
    bool validate_ref(const MemoRef& ref) const override;

    // ---------------------------------------------------------------------
    // x64 bridge helpers
    //
    // These helpers expose raw object-id operations so the x64 DBF layer can
    // store an 8-byte memo link in-row while still using this backend.
    // They do NOT change the existing token/ref interface.
    // ---------------------------------------------------------------------

    // Append a new text object and return its raw object id.
    // Returns 0 on failure. Optionally fills err.
    std::uint64_t put_text_id(std::string_view text, std::string* err = nullptr);

    // Read text by raw object id. Returns false on failure.
    bool get_text_id(std::uint64_t object_id, std::string& out, std::string* err = nullptr);

    // Append a new version from old object id and return the new raw object id.
    // old_object_id == 0 means "create fresh".
    // Returns false on failure.
    bool update_text_id(std::uint64_t old_object_id,
                        std::string_view text,
                        std::uint64_t& new_object_id,
                        std::string* err = nullptr);

    // Tombstone by raw object id.
    bool erase_id(std::uint64_t object_id, std::string* err = nullptr);

    // True if this object id is currently live.
    bool exists_id(std::uint64_t object_id) const noexcept;

    // Convert raw object id <-> backend-neutral token ref
    MemoRef ref_from_object_id(std::uint64_t object_id) const;
    bool try_object_id_from_ref(const MemoRef& ref, std::uint64_t& object_id) const noexcept;

    // ---------------------------------------------------------------------
    // Verification / GC helpers
    // ---------------------------------------------------------------------

    // Enumerate all currently live object ids.
    bool list_live_ids(std::vector<std::uint64_t>& out_ids) const;

    // Return payload size for a live object id.
    bool get_object_size_id(std::uint64_t object_id,
                            std::uint64_t& out_bytes,
                            std::string* err = nullptr) const;

    // Cheap live-object count.
    std::uint64_t live_object_count() const noexcept;

    // Current logical append size of the DTX file.
    std::uint64_t append_bytes() const noexcept;

    // DTX-specific helpers
    bool create_empty(const std::string& sidecar_path);
    bool header_valid() const noexcept;

private:
    struct ObjectLoc {
        std::uint64_t header_offset {0};
        std::uint64_t payload_offset {0};
        std::uint64_t payload_bytes {0};
        std::uint16_t state {0};
        std::uint16_t kind {0};
    };

    std::fstream _fp;
    std::string _path;
    dtx::DtxHeader _hdr{};

    // v1 in-memory object map built on open/scan
    std::unordered_map<std::uint64_t, ObjectLoc> _live_index;

private:
    // open/create internals
    bool open_existing_(const std::string& sidecar_path);
    bool create_new_(const std::string& sidecar_path);
    bool load_header_();
    bool write_header_();
    bool rebuild_index_();

    // object I/O
    std::uint64_t allocate_object_id_();
    std::uint64_t append_text_object_(std::string_view text,
                                      std::uint64_t previous_version_of = 0);
    bool read_object_text_(std::uint64_t object_id, std::string& out);
    bool tombstone_object_(std::uint64_t object_id);

    // ref conversion
    MemoRef make_ref_from_object_id_(std::uint64_t object_id) const;
    bool try_parse_ref_(const MemoRef& ref, std::uint64_t& object_id) const noexcept;

    // small helpers
    static std::uint64_t file_size_(std::fstream& fp);
};

} // namespace dottalk::memo
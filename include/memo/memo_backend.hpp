// memo_backend.hpp
// Backend-neutral memo interface for DotTalk++
//
// Purpose:
//   - decouple DBF/xBase memo field handling from the on-disk sidecar format
//   - allow DTX now and FPT later
//   - keep row-level memo logic stable while backend implementations evolve
//
// Current intended backend:
//   - DTX (native DotTalk++ memo store)
//
// Future backend:
//   - FPT (Fox/VFP compatibility)
//
// Notes:
//   - DBF rows should store a memo reference token, not the payload itself
//   - DTX v1 canonical external token is 16-char uppercase hex
//   - blank token means "no memo"

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dottalk::memo {

// -----------------------------------------------------------------------------
// Backend kind
// -----------------------------------------------------------------------------
enum class BackendKind : std::uint8_t {
    None = 0,
    DTX,
    FPT
};

// -----------------------------------------------------------------------------
// Payload kind
// Keep small for now. Text first.
// -----------------------------------------------------------------------------
enum class MemoPayloadKind : std::uint8_t {
    Unknown = 0,
    TextUtf8,
    Binary
};

// -----------------------------------------------------------------------------
// Open/create policy
// -----------------------------------------------------------------------------
enum class OpenMode : std::uint8_t {
    OpenExisting = 0,
    CreateIfMissing,
    TruncateAndCreate
};

// -----------------------------------------------------------------------------
// Reference token held in-row by the DBF layer.
//
// This is backend-neutral at the interface level.
// The backend decides how to interpret/encode it.
// -----------------------------------------------------------------------------
struct MemoRef {
    std::string token;   // blank => no memo

    [[nodiscard]] bool empty() const noexcept {
        return token.empty();
    }

    static MemoRef nullref() {
        return MemoRef{};
    }
};

// -----------------------------------------------------------------------------
// Result/status helpers
// -----------------------------------------------------------------------------
struct MemoStat {
    bool            exists         {false};
    MemoPayloadKind kind           {MemoPayloadKind::Unknown};
    std::uint64_t   logical_bytes  {0};
    std::uint64_t   physical_bytes {0};
};

struct MemoPutResult {
    bool        ok      {false};
    MemoRef     ref     {};
    std::string error   {};
};

struct MemoGetResult {
    bool                    ok      {false};
    std::string             text    {};
    std::vector<std::byte>  bytes   {};
    std::string             error   {};
};

struct MemoOpResult {
    bool        ok    {false};
    std::string error {};
};

// -----------------------------------------------------------------------------
// Backend-neutral memo interface
// -----------------------------------------------------------------------------
class IMemoBackend {
public:
    virtual ~IMemoBackend() = default;

    virtual BackendKind kind() const noexcept = 0;

    virtual bool is_open() const noexcept = 0;
    virtual std::string path() const = 0;

    virtual MemoOpResult open(const std::string& sidecar_path, OpenMode mode) = 0;
    virtual MemoOpResult flush() = 0;
    virtual void close() noexcept = 0;

    // Basic text-first API
    virtual MemoPutResult put_text(std::string_view text) = 0;
    virtual MemoGetResult get_text(const MemoRef& ref) = 0;
    virtual MemoPutResult update_text(const MemoRef& old_ref, std::string_view text) = 0;
    virtual MemoOpResult  erase(const MemoRef& ref) = 0;
    virtual MemoStat      stat(const MemoRef& ref) = 0;

    // Backend-neutral token helpers
    virtual MemoRef make_null_ref() const = 0;
    virtual bool is_null_ref(const MemoRef& ref) const = 0;
    virtual bool validate_ref(const MemoRef& ref) const = 0;
};

// -----------------------------------------------------------------------------
// Optional convenience owner
// Keeps one backend pointer without imposing policy.
// -----------------------------------------------------------------------------
class MemoBackendHandle {
public:
    MemoBackendHandle() = default;
    explicit MemoBackendHandle(std::unique_ptr<IMemoBackend> backend)
        : _backend(std::move(backend)) {}

    [[nodiscard]] bool attached() const noexcept {
        return static_cast<bool>(_backend);
    }

    [[nodiscard]] IMemoBackend* get() noexcept {
        return _backend.get();
    }

    [[nodiscard]] const IMemoBackend* get() const noexcept {
        return _backend.get();
    }

    void reset(std::unique_ptr<IMemoBackend> backend = {}) noexcept {
        _backend = std::move(backend);
    }

    IMemoBackend& operator*() noexcept { return *_backend; }
    const IMemoBackend& operator*() const noexcept { return *_backend; }

    IMemoBackend* operator->() noexcept { return _backend.get(); }
    const IMemoBackend* operator->() const noexcept { return _backend.get(); }

private:
    std::unique_ptr<IMemoBackend> _backend;
};

// -----------------------------------------------------------------------------
// Integration helpers
// These are intentionally small and policy-light.
// -----------------------------------------------------------------------------
inline bool memo_field_type(char dbf_type) noexcept {
    return dbf_type == 'M' || dbf_type == 'm';
}

inline MemoRef memo_ref_from_field_text(std::string_view raw) {
    // Caller can trim if desired; backend decides ultimate validity.
    return MemoRef{std::string(raw)};
}

inline std::string memo_field_text_from_ref(const MemoRef& ref) {
    return ref.token;
}

} // namespace dottalk::memo
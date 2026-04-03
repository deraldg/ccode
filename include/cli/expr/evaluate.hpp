#pragma once
// Central predicate facade for DotTalk++ commands.
// Compile once per command, then eval per record.

#include <string>
#include <string_view>

namespace xbase { class DbArea; }

namespace dottalk::expr {

struct Predicate {
    void* impl{nullptr};
    bool  (*eval)(void* impl, const xbase::DbArea& area){nullptr};
    void  (*destroy)(void* impl){nullptr};

    explicit operator bool() const { return impl && eval; }
    void reset() {
        if (impl && destroy) destroy(impl);
        impl = nullptr; eval = nullptr; destroy = nullptr;
    }
    ~Predicate(){ reset(); }
    Predicate() = default;
    Predicate(const Predicate&) = delete;
    Predicate& operator=(const Predicate&) = delete;
    Predicate(Predicate&& o) noexcept { *this = std::move(o); }
    Predicate& operator=(Predicate&& o) noexcept {
        if (this != &o) {
            reset();
            impl = o.impl; eval = o.eval; destroy = o.destroy;
            o.impl = nullptr; o.eval = nullptr; o.destroy = nullptr;
        }
        return *this;
    }
};

struct CompileOut {
    Predicate pred;
    std::string error; // empty if ok
};

/// Compiles an expression text into a runnable predicate bound to `area`'s schema.
/// Accepts raw tails (e.g., "MAJOR = 'PHYS' AND GPA >= 3").
/// Empty/whitespace expressions compile to a predicate that always returns true.
CompileOut compile_predicate(std::string expr_text, const xbase::DbArea& area);

/// Convenience: returns a predicate that always returns `value`.
CompileOut compile_literal(bool value);

} // namespace dottalk::expr




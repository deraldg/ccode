#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

// Minimal, header-only surface for the TUPTALK tuple buffer.
// Thread-safe enough for CLI use via a single global instance.

struct TupEntry {
    // FoxPro-like field spec for a tuple cell
    // Types: C (char), N (numeric), D (date), L (logical)
    char        type{'C'};
    int         length{10};
    int         decimals{0};
    std::string value;
};

class TupTalkBuffer {
public:
    static TupTalkBuffer& instance() {
        static TupTalkBuffer g;
        return g;
    }

    void clear() {
        std::lock_guard<std::mutex> lk(mu_);
        rows_.clear();
    }

    // Adds an entry; returns index of the new entry (0-based)
    std::size_t add(char type, int length, int decimals, std::string value) {
        std::lock_guard<std::mutex> lk(mu_);
        TupEntry e;
        e.type     = type;
        e.length   = (length <= 0 ? 1 : length);
        e.decimals = (decimals <  0 ? 0 : decimals);
        e.value    = std::move(value);
        rows_.push_back(std::move(e));
        return rows_.size() - 1;
    }

    bool erase(std::size_t index) {
        std::lock_guard<std::mutex> lk(mu_);
        if (index >= rows_.size()) return false;
        rows_.erase(rows_.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lk(mu_);
        return rows_.size();
    }

    // Snapshot for read/display
    std::vector<TupEntry> snapshot() const {
        std::lock_guard<std::mutex> lk(mu_);
        return rows_;
    }

private:
    TupTalkBuffer() = default;
    mutable std::mutex   mu_;
    std::vector<TupEntry> rows_;
};




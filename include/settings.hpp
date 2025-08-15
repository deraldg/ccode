#pragma once
#include <atomic>

namespace cli {

// Global, lightweight runtime settings for the CLI layer.
// Defaults follow classic FoxPro expectations: deleted = ON (hidden).
struct Settings {
    std::atomic<bool> deleted_on{true}; // ON => hide deleted records

    static Settings& instance() {
        static Settings s;
        return s;
    }

    // helper accessors
    static bool deletedOn()  { return instance().deleted_on.load(); }
    static void setDeleted(bool on) { instance().deleted_on.store(on); }
};

} // namespace cli

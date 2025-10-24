#include "cli/memo_auto.hpp"
#include <unordered_map>
#include <filesystem>

namespace fs = std::filesystem;
using xbase::memo::MemoStore;

namespace cli_memo {

static MemoConfig g_cfg{};
static std::unordered_map<xbase::DbArea*, std::unique_ptr<MemoStore>> g_store;

void set_memo_config(const MemoConfig& cfg) { g_cfg = cfg; }
MemoConfig get_memo_config() { return g_cfg; }

static std::string basepath_from(const std::string& openedPath) {
    fs::path p(openedPath);
    if (!p.has_extension()) return p.string();            // e.g., data\students
    if (p.extension() == ".dbf" || p.extension() == ".DBF") {
        return p.replace_extension().string();            // strip .dbf
    }
    return p.string();                                    // unknown ext: treat as base
}

static bool file_exists(const std::string& path) {
    std::error_code ec; return fs::exists(fs::path(path), ec);
}

xbase::memo::MemoStore* memo_store_for(xbase::DbArea& area) {
    auto it = g_store.find(&area);
    return (it == g_store.end()) ? nullptr : it->second.get();
}

void memo_auto_on_close(xbase::DbArea& area) {
    auto it = g_store.find(&area);
    if (it != g_store.end()) {
        // dtor flushes; if you want explicit flush: it->second->flush();
        g_store.erase(it);
    }
}

bool memo_auto_on_use(xbase::DbArea& area,
                      const std::string& openedPath,
                      bool hasMemoFields,
                      std::string& err)
{
    // If we’re replacing a table in the same area, ensure old sidecar is closed
    memo_auto_on_close(area);

    if (!hasMemoFields) {
        // No memo fields: nothing to attach.
        return true;
    }

    // Compute sidecar path
    std::string base = basepath_from(openedPath);
    fs::path dtx = fs::path(base).replace_extension(".dtx");

    try {
        if (file_exists(dtx.string())) {
            g_store[&area] = std::make_unique<MemoStore>(MemoStore::open(base));
            return true;
        }
        // Missing sidecar
        if (g_cfg.autocreate) {
            g_store[&area] = std::make_unique<MemoStore>(MemoStore::create(base));
            return true;
        }
        if (g_cfg.strict) {
            err = "Memo sidecar missing: " + dtx.string();
            return false;
        }
        // Not strict, not autocreate: continue without memo store
        return true;
    } catch (const std::exception& ex) {
        err = ex.what();
        return false;
    }
}

} // namespace cli_memo

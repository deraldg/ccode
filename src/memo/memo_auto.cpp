#include "memo/memo_auto.hpp"

#include <filesystem>
#include <memory>
#include <unordered_map>

#include "memo/memostore.hpp"

namespace fs = std::filesystem;

namespace cli_memo {

static MemoConfig g_cfg{};
static std::unordered_map<xbase::DbArea*, std::unique_ptr<dottalk::memo::IMemoBackend>> g_store;

void set_memo_config(const MemoConfig& cfg) { g_cfg = cfg; }
MemoConfig get_memo_config() { return g_cfg; }

static std::string sidecar_path_from_opened_path(const std::string& openedPath)
{
    fs::path p(openedPath);
    if (!p.has_extension()) {
        p.replace_extension(".dtx");
        return p.string();
    }

    const auto ext = p.extension().string();
    if (ext == ".dbf" || ext == ".DBF") {
        p.replace_extension(".dtx");
        return p.string();
    }

    // If caller already passed some odd extension, still normalize to .dtx
    p.replace_extension(".dtx");
    return p.string();
}

static bool file_exists(const std::string& path)
{
    std::error_code ec;
    return fs::exists(fs::path(path), ec);
}

dottalk::memo::IMemoBackend* memo_backend_for(xbase::DbArea& area)
{
    auto it = g_store.find(&area);
    return (it == g_store.end()) ? nullptr : it->second.get();
}

void memo_auto_on_close(xbase::DbArea& area)
{
    auto it = g_store.find(&area);
    if (it != g_store.end()) {
        if (it->second) {
            it->second->close();
        }
        g_store.erase(it);
    }
}

bool memo_auto_on_use(xbase::DbArea& area,
                      const std::string& openedPath,
                      bool hasMemoFields,
                      std::string& err)
{
    // If replacing a table in the same area, ensure old sidecar is closed.
    memo_auto_on_close(area);

    if (!hasMemoFields) {
        return true;
    }

    const std::string dtxPath = sidecar_path_from_opened_path(openedPath);

    try {
        std::unique_ptr<dottalk::memo::IMemoBackend> store =
            std::make_unique<dottalk::memo::MemoStore>();

        dottalk::memo::MemoOpResult r{};
        if (file_exists(dtxPath)) {
            r = store->open(dtxPath, dottalk::memo::OpenMode::OpenExisting);
            if (!r.ok) {
                err = "Memo sidecar open failed: " + dtxPath +
                      (r.error.empty() ? "" : " (" + r.error + ")");
                return false;
            }
            g_store[&area] = std::move(store);
            return true;
        }

        // Missing sidecar
        if (g_cfg.autocreate) {
            r = store->open(dtxPath, dottalk::memo::OpenMode::CreateIfMissing);
            if (!r.ok) {
                err = "Memo sidecar create failed: " + dtxPath +
                      (r.error.empty() ? "" : " (" + r.error + ")");
                return false;
            }
            g_store[&area] = std::move(store);
            return true;
        }

        if (g_cfg.strict) {
            err = "Memo sidecar missing: " + dtxPath;
            return false;
        }

        // Not strict, not autocreate: continue without memo backend
        return true;
    }
    catch (const std::exception& ex) {
        err = ex.what();
        return false;
    }
}

} // namespace cli_memo
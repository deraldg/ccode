#include "memo/memo_auto.hpp"

#include <filesystem>
#include <memory>
#include <unordered_map>

#include "memo/memostore.hpp"

namespace fs = std::filesystem;

namespace cli_memo {

static MemoConfig g_cfg{};
static std::unordered_map<xbase::DbArea*, dottalk::memo::MemoBackendHandle> g_store;

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

    p.replace_extension(".dtx");
    return p.string();
}

static bool file_exists(const std::string& path)
{
    std::error_code ec;
    return fs::exists(fs::path(path), ec);
}

static std::unique_ptr<dottalk::memo::IMemoBackend> create_default_backend()
{
    return std::make_unique<dottalk::memo::MemoStore>();
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
        if (it->second.attached()) {
            (void)it->second->flush();
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
    memo_auto_on_close(area);

    if (!hasMemoFields) {
        return true;
    }

    const std::string dtxPath = sidecar_path_from_opened_path(openedPath);

    try {
        auto backend = create_default_backend();

        using namespace dottalk::memo;

        MemoOpResult r{};
        if (file_exists(dtxPath)) {
            r = backend->open(dtxPath, OpenMode::OpenExisting);
            if (!r.ok) {
                err = "Memo sidecar open failed: " + dtxPath +
                      (r.error.empty() ? "" : " (" + r.error + ")");
                return false;
            }
            g_store[&area].reset(std::move(backend));
            return true;
        }

        if (g_cfg.autocreate) {
            r = backend->open(dtxPath, OpenMode::CreateIfMissing);
            if (!r.ok) {
                err = "Memo sidecar create failed: " + dtxPath +
                      (r.error.empty() ? "" : " (" + r.error + ")");
                return false;
            }
            g_store[&area].reset(std::move(backend));
            return true;
        }

        if (g_cfg.strict) {
            err = "Memo sidecar missing: " + dtxPath;
            return false;
        }

        return true;
    }
    catch (const std::exception& ex) {
        err = ex.what();
        return false;
    }
}

} // namespace cli_memo
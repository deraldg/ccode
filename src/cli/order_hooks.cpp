// src/cli/order_hooks.cpp
#include "cli/order_hooks.hpp"
#include "cli/order_state.hpp"
#include "cli/order_nav.hpp"

#include <filesystem>
#include <string>
#include <vector>

#include "cnx/cnx.hpp"
#include "cdx/cdx.hpp"

namespace fs = std::filesystem;

namespace orderhooks {

static constexpr uint32_t TAGF_UNIQUE = 0x0001; // CNX/CDX tag flags bit for UNIQUE (keep in sync with cnx)

static std::string up_copy(std::string s)
{
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

static bool cnx_pick_default_tag(const std::string& cnx_path, std::string& out_tag_upper)
{
    out_tag_upper.clear();

    cnxfile::CNXHandle* h = nullptr;
    if (!cnxfile::open(cnx_path, h) || !h) {
        return false;
    }

    std::vector<cnxfile::TagInfo> tags;
    const bool ok = cnxfile::read_tagdir(h, tags);
    cnxfile::close(h);

    if (!ok || tags.empty()) {
        return false;
    }

    // Prefer UNIQUE tag if present; otherwise first tag.
    for (const auto& t : tags) {
        if ((t.flags & TAGF_UNIQUE) != 0 && !t.name.empty()) {
            out_tag_upper = up_copy(t.name);
            return true;
        }
    }

    out_tag_upper = up_copy(tags.front().name);
    return !out_tag_upper.empty();
}

static bool cdx_pick_default_tag(const std::string& cdx_path, std::string& out_tag_upper)
{
    out_tag_upper.clear();

    cdxfile::CDXHandle* h = nullptr;
    if (!cdxfile::open(cdx_path, h) || !h) {
        return false;
    }

    std::vector<cdxfile::TagInfo> tags;
    const bool ok = cdxfile::read_tagdir(h, tags);
    cdxfile::close(h);

    if (!ok || tags.empty()) {
        return false;
    }

    // Prefer UNIQUE tag if present; otherwise first tag.
    for (const auto& t : tags) {
        if ((t.flags & TAGF_UNIQUE) != 0 && !t.name.empty()) {
            out_tag_upper = up_copy(t.name);
            return true;
        }
    }

    out_tag_upper = up_copy(tags.front().name);
    return !out_tag_upper.empty();
}

static bool cnx_tag_exists(const std::string& cnx_path, const std::string& tag_upper)
{
    if (tag_upper.empty()) {
        return false;
    }

    cnxfile::CNXHandle* h = nullptr;
    if (!cnxfile::open(cnx_path, h) || !h) {
        return false;
    }

    std::vector<cnxfile::TagInfo> tags;
    const bool ok = cnxfile::read_tagdir(h, tags);
    cnxfile::close(h);

    if (!ok) {
        return false;
    }

    for (const auto& t : tags) {
        if (up_copy(t.name) == tag_upper) {
            return true;
        }
    }
    return false;
}

static bool cdx_tag_exists(const std::string& cdx_path, const std::string& tag_upper)
{
    if (tag_upper.empty()) {
        return false;
    }

    cdxfile::CDXHandle* h = nullptr;
    if (!cdxfile::open(cdx_path, h) || !h) {
        return false;
    }

    std::vector<cdxfile::TagInfo> tags;
    const bool ok = cdxfile::read_tagdir(h, tags);
    cdxfile::close(h);

    if (!ok) {
        return false;
    }

    for (const auto& t : tags) {
        if (up_copy(t.name) == tag_upper) {
            return true;
        }
    }
    return false;
}


void reconcile_after_mutation(xbase::DbArea& area) noexcept
{
    try {
        // Any mutation can invalidate cached endpoints / lists.
        order_nav_invalidate(area);

        const std::string name = orderstate::orderName(area);
        if (name.empty()) {
            return;
        }

        // If the backing container vanished, clear order state.
        std::error_code ec;
        if (!fs::exists(fs::path(name), ec) || ec) {
            orderstate::clearOrder(area);
            return;
        }

        // CNX must have a valid active tag.
        if (orderstate::isCnx(area)) {
            std::string tag = up_copy(orderstate::activeTag(area));

            if (!tag.empty() && cnx_tag_exists(name, tag)) {
                // Keep as-is.
                orderstate::setActiveTag(area, tag);
                return;
            }

            // If tag missing/invalid, pick a default from the CNX tagdir.
            std::string chosen;
            if (cnx_pick_default_tag(name, chosen)) {
                orderstate::setActiveTag(area, chosen);
                return;
            }

            // CNX unusable => clear order (SETORDER will report failure).
            orderstate::clearOrder(area);
        }
        // CDX must have a valid active tag.
        if (orderstate::isCdx(area)) {
            std::string tag = up_copy(orderstate::activeTag(area));

            if (!tag.empty() && cdx_tag_exists(name, tag)) {
                // Keep as-is.
                orderstate::setActiveTag(area, tag);
                return;
            }

            // If tag missing/invalid, pick a default from the CDX tagdir.
            std::string chosen;
            if (cdx_pick_default_tag(name, chosen)) {
                orderstate::setActiveTag(area, chosen);
                return;
            }

            // CDX unusable => clear order (SETORDER will report failure).
            orderstate::clearOrder(area);
        }


    }
    catch (...) {
        // never throw
    }
}

void auto_top(xbase::DbArea& area) noexcept
{
    try {
        if (!area.isOpen()) {
            return;
        }

        int32_t rn = 0;
        if (order_first_recno(area, rn) && rn >= 1 && rn <= area.recCount()) {
            if (area.gotoRec(rn)) {
                (void)area.readCurrent();
            }
            return;
        }

        if (area.top()) {
            (void)area.readCurrent();
        }
    }
    catch (...) {
        // never throw
    }
}

void attach_default_order(xbase::DbArea& area) noexcept
{
    (void)area;
    // No-op for now. Auto-attach is handled in USE; SETORDER explicitly sets an order.
}

} // namespace orderhooks

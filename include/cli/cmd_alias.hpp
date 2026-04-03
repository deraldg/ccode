#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

namespace cli {

/**
 * Lightweight in-memory alias registry for work areas.
 * Aliases are case-insensitive. We store them normalized (upper).
 * Values are current area numbers (0-based).
 *
 * NOTE: Aliases don?t auto-update if an area closes/reopens.
 * We keep the mapping as-is (may become dangling). The resolver
 * reports dangling aliases so callers can emit a helpful message.
 */
class AliasRegistry {
public:
    // Add or update alias -> area
    void set(std::string alias, int area);

    // Remove alias; returns true if existed
    bool erase(std::string alias);

    // Remove all aliases
    void clear();

    // Resolve alias to area number if present
    std::optional<int> resolve(std::string alias) const;

    // Snapshot for listing
    struct Item { std::string alias; int area; };
    std::vector<Item> list() const;

private:
    static std::string normalize(std::string s);
    std::unordered_map<std::string, int> map_; // alias(UPPER) -> area#
};

// Global singleton accessors (simple, sufficient for CLI)
AliasRegistry& aliases();

} // namespace cli




#pragma once

#include <memory>
#include <unordered_map>
#include <functional>
#include <string>
#include <vector>

#include "xindex/index_tag.hpp"  // ensure complete type for unique_ptr

namespace xbase { class DbArea; }

namespace xindex {

struct IndexSpec;
struct IndexKey;
struct KeyRec;

class IndexManager {
public:
    explicit IndexManager(xbase::DbArea& area);
    ~IndexManager();

    // Lifecycle
    bool load_for_table(const std::string& dbfPath);
    bool save(const std::string& dbfPath);

    // Index ops
    IndexTag& ensure_tag(const IndexSpec& spec);
    bool      set_active(const std::string& tag);
    void      clear_active();
    bool      has_active() const;
    IndexTag*       active();
    const IndexTag* active() const;
    void      set_direction(bool ascending);
    int       top()    const;
    int       bottom() const;

    // Mutations
    void on_append(int recno);
    void on_replace(int recno);
    void on_delete(int recno);
    void on_recall(int recno);
    void on_pack(std::function<int(int)> recnoRemap);
    void on_zap();

    // Persistence (stubs for now)
    bool        save_json(const std::string& path);
    bool        load_json(const std::string& path);
    std::string inx_path(const std::string& dbfPath) const;

    // Introspection
    std::vector<std::string> listTags() const;            // tag names
    std::string              exprFor(const std::string& tag) const; // human-readable expr from fields

    // Back-compat shims (so existing CLI calls compile)
    std::vector<std::string> list_tags() const { return listTags(); }
    std::string              tag_expr(const std::string& t) const { return exprFor(t); }

    // Helpers
    static std::string trim(std::string s);
    static std::string up(std::string s);
    IndexKey           make_key_from_tokens(const IndexSpec& spec, const std::vector<std::string>& toks) const;
    IndexKey           key_from_record(const IndexSpec& spec, int recno) const;

private:
    xbase::DbArea& area_;
    std::unordered_map<std::string, std::unique_ptr<IndexTag>> tags_;
    std::string active_tag_;
    bool dir_ascending_{true};
    bool dirty_{false};
};

} // namespace xindex

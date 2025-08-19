#pragma once
#include "xindex/index_tag.hpp"
#include <memory>
#include <unordered_map>
#include <functional>
#include <string>

namespace xbase { class DbArea; }

namespace xindex {

class IndexManager {
public:
    explicit IndexManager(xbase::DbArea& area);

    bool load_for_table(std::string const& dbfPath);
    bool save(std::string const& dbfPath);

    IndexTag& ensure_tag(IndexSpec const& spec);

    bool set_active(std::string const& tag);
    void clear_active();
    bool has_active() const;
    IndexTag* active();
    const IndexTag* active() const;

    void set_direction(bool ascending);
    bool direction_ascending() const { return dir_ascending_; }

    int top() const;
    int bottom() const;

    void on_append(int recno);
    void on_replace(int recno);
    void on_delete(int recno);
    void on_recall(int recno);
    void on_pack(std::function<int(int)> recnoRemap);
    void on_zap();

    IndexKey make_key_from_tokens(IndexSpec const& spec, const std::vector<std::string>& toks) const;

private:
    xbase::DbArea& area_;
    std::unordered_map<std::string, std::unique_ptr<IndexTag>> tags_;
    std::string active_tag_;
    bool dir_ascending_ = true;
    bool dirty_ = false;

    std::string inx_path(std::string const& dbfPath) const;
    IndexKey key_from_record(IndexSpec const& spec, int recno) const;

    static std::string trim(std::string s);
    static std::string up(std::string s);

    bool save_json(std::string const& path);
    bool load_json(std::string const& path);
};

} // namespace xindex

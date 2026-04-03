#pragma once
#include <string>
#include <unordered_map>
#include <functional>

namespace xbase { class DbArea; }

namespace dli {

struct FieldUpdate {
    std::string name;
    std::string value;
};

struct EditSession {
    xbase::DbArea* db {nullptr};
    long long recno {0};
    std::unordered_map<std::string, std::string> staged;
    bool active {false};
};

struct Hooks {
    std::function<char(xbase::DbArea&, const std::string&)> field_type;
    std::function<bool(xbase::DbArea&, long long original_recno,
                       const std::unordered_map<std::string,std::string>& staged)> after_commit_reposition;
    std::function<void(xbase::DbArea&)> refresh_row;
};

void set_hooks(const Hooks& h);
EditSession begin_edit(xbase::DbArea& db);
bool stage(EditSession& es, const std::string& fieldName, const std::string& rawText);
bool commit(EditSession& es, std::string& err);
void cancel(EditSession& es);

} // namespace dli
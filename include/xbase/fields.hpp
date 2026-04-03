#pragma once

#include "xbase.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace fields {

enum class Op {
    None = 0,
    Show,
    Append,
    DeleteField,
    ModifyName,
    ModifyType,
    ModifyTo,
    CopyTo,
    CopyToMap,
    Validate,
    Check,
    RebuildIndexes
};

enum class Status {
    Ok = 0,
    InvalidArgument,
    InvalidState,
    Unsupported,
    NotImplemented,
    Failed
};

enum class IndexImpact {
    None = 0,
    Unknown,
    RebuildRecommended,
    RebuildRequired,
    Blocked
};

struct Result {
    Status      status{Status::Ok};
    IndexImpact indexImpact{IndexImpact::None};
    bool        changed{false};
    bool        rebuildSuggested{false};
    std::string message;
};

struct FieldProtectionInfo {
    bool isIndexed{false};
    bool isUnique{false};
    bool isPrimaryKey{false};
};

struct AppendOptions {
    bool rebuildIndexesIfPossible{false};
    bool failIfIndexesPresent{false};
};

struct CopyMapEntry {
    int         srcIndex{-1};
    int         dstIndex{-1};
    std::string dstName;
};

struct CopyPlan {
    std::vector<CopyMapEntry> entries;
};

std::string usage();
std::string opName(Op op);

bool parseFieldSpec(const std::string& text, xbase::FieldDef& out, std::string& err);
bool validateFieldName(const std::string& name, std::string& err);
bool validateFieldDef(const xbase::FieldDef& fd, std::string& err);

int  findFieldCI(const xbase::DbArea& db, const std::string& name);
bool hasFieldCI(const xbase::DbArea& db, const std::string& name);

FieldProtectionInfo getFieldProtectionInfo(const xbase::DbArea& db,
                                           const std::string& fieldName);

Result show(const xbase::DbArea& db);

Result append(xbase::DbArea& db,
              const xbase::FieldDef& fd,
              const AppendOptions& opts = {});

Result deleteField(xbase::DbArea& db, const std::string& fieldName);
Result modifyName(xbase::DbArea& db,
                  const std::string& oldName,
                  const std::string& newName);
Result modifyType(xbase::DbArea& db,
                  const std::string& fieldName,
                  const xbase::FieldDef& newDef);
Result modifyTo(xbase::DbArea& db,
                const std::string& oldName,
                const xbase::FieldDef& newDef);

Result copyTo(xbase::DbArea& db, const std::string& targetPath);
Result copyToMap(xbase::DbArea& db,
                 const std::string& targetPath,
                 const CopyPlan& plan);

Result validate(const xbase::DbArea& db);
Result check(const xbase::DbArea& db);
Result rebuildIndexes(xbase::DbArea& db);

IndexImpact assessAppendIndexImpact(const xbase::DbArea& db,
                                    const xbase::FieldDef& fd);

} // namespace fields

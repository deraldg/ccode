#pragma once

#include <filesystem>
#include <string>

namespace dottalk::paths {

namespace fs = std::filesystem;

enum class Slot {
    DATA,
    DBF,
    xDBF,
    INDEXES,
    LMDB,
    WORKSPACES,
    SCHEMAS,
    PROJECTS,
    SCRIPTS,
    TESTS,
    HELP,
    LOGS,
    TMP
};

struct PathState {
    fs::path data_root;

    fs::path dbf;
    fs::path xdbf;
    fs::path indexes;
    fs::path lmdb;
    fs::path workspaces;
    fs::path schemas;
    fs::path projects;
    fs::path scripts;
    fs::path tests;
    fs::path help;
    fs::path logs;
    fs::path tmp;
};

PathState& state();

fs::path get_slot(Slot slot);
void set_slot(Slot slot, const fs::path& value);
void reset();

const char* slot_name(Slot slot);
bool slot_from_name(const std::string& text, Slot& out);
bool slot_from_string(const std::string& text, Slot& out);

} // namespace dottalk::paths
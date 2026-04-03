#include "common/path_state.hpp"

#include <algorithm>
#include <cctype>

namespace dottalk::paths {

namespace {

PathState g_state;

std::string upper_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

} // namespace

PathState& state() {
    return g_state;
}

fs::path get_slot(Slot slot) {
    switch (slot) {
    case Slot::DATA:       return g_state.data_root;
    case Slot::DBF:        return g_state.dbf;
    case Slot::xDBF:       return g_state.xdbf;
    case Slot::INDEXES:    return g_state.indexes;
    case Slot::LMDB:       return g_state.lmdb;
    case Slot::WORKSPACES: return g_state.workspaces;
    case Slot::SCHEMAS:    return g_state.schemas;
    case Slot::PROJECTS:   return g_state.projects;
    case Slot::SCRIPTS:    return g_state.scripts;
    case Slot::TESTS:      return g_state.tests;
    case Slot::HELP:       return g_state.help;
    case Slot::LOGS:       return g_state.logs;
    case Slot::TMP:        return g_state.tmp;
    default:               return {};
    }
}

void set_slot(Slot slot, const fs::path& value) {
    switch (slot) {
    case Slot::DATA:       g_state.data_root  = value; break;
    case Slot::DBF:        g_state.dbf        = value; break;
    case Slot::xDBF:       g_state.xdbf       = value; break;
    case Slot::INDEXES:    g_state.indexes    = value; break;
    case Slot::LMDB:       g_state.lmdb       = value; break;
    case Slot::WORKSPACES: g_state.workspaces = value; break;
    case Slot::SCHEMAS:    g_state.schemas    = value; break;
    case Slot::PROJECTS:   g_state.projects   = value; break;
    case Slot::SCRIPTS:    g_state.scripts    = value; break;
    case Slot::TESTS:      g_state.tests      = value; break;
    case Slot::HELP:       g_state.help       = value; break;
    case Slot::LOGS:       g_state.logs       = value; break;
    case Slot::TMP:        g_state.tmp        = value; break;
    default: break;
    }
}

void reset() {
    g_state = PathState{};
}

const char* slot_name(Slot slot) {
    switch (slot) {
    case Slot::DATA:       return "DATA";
    case Slot::DBF:        return "DBF";
    case Slot::xDBF:       return "XDBF";
    case Slot::INDEXES:    return "INDEXES";
    case Slot::LMDB:       return "LMDB";
    case Slot::WORKSPACES: return "WORKSPACES";
    case Slot::SCHEMAS:    return "SCHEMAS";
    case Slot::PROJECTS:   return "PROJECTS";
    case Slot::SCRIPTS:    return "SCRIPTS";
    case Slot::TESTS:      return "TESTS";
    case Slot::HELP:       return "HELP";
    case Slot::LOGS:       return "LOGS";
    case Slot::TMP:        return "TMP";
    default:               return "?";
    }
}

bool slot_from_name(const std::string& text, Slot& out) {
    const std::string u = upper_copy(text);

    if (u == "DATA") { out = Slot::DATA; return true; }
    if (u == "DBF") { out = Slot::DBF; return true; }
    if (u == "XDBF") { out = Slot::xDBF; return true; }
    if (u == "INDEX" || u == "INDEXES") { out = Slot::INDEXES; return true; }
    if (u == "LMDB") { out = Slot::LMDB; return true; }
    if (u == "WORKSPACE" || u == "WORKSPACES") { out = Slot::WORKSPACES; return true; }
    if (u == "SCHEMA" || u == "SCHEMAS") { out = Slot::SCHEMAS; return true; }
    if (u == "PROJECT" || u == "PROJECTS") { out = Slot::PROJECTS; return true; }
    if (u == "SCRIPT" || u == "SCRIPTS") { out = Slot::SCRIPTS; return true; }
    if (u == "TEST" || u == "TESTS") { out = Slot::TESTS; return true; }
    if (u == "HELP") { out = Slot::HELP; return true; }
    if (u == "LOG" || u == "LOGS") { out = Slot::LOGS; return true; }
    if (u == "TMP" || u == "TEMP") { out = Slot::TMP; return true; }

    return false;
}

bool slot_from_string(const std::string& text, Slot& out) {
    return slot_from_name(text, out);
}

} // namespace dottalk::paths
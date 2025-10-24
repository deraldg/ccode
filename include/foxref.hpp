#pragma once
#include <string>
#include <vector>
#include <string_view>

namespace foxref {

struct Item {
    const char* name;       // canonical upper-case command name
    const char* syntax;     // short syntax line
    const char* summary;    // one-liner
    bool supported;         // whether DotTalk++ currently supports it
};

// Minimal seed set; extend anytime.
inline const std::vector<Item>& catalog() {
    static const std::vector<Item> k = {
        // Core you already implement
        {"USE",        "USE <table> [IN <n>]",                 "Open a DBF in a work area.", true},
        {"FIELDS",     "FIELDS",                               "Show structure (field list).", true},
        {"INDEX",      "INDEX ON <field|#n> TAG <name>",       "Create simple .INX index.", true},
        {"SETINDEX",   "SETINDEX <tag|path>",                  "Activate an index for the area.", true},
        {"LIST",       "LIST [ALL|<n>] [FOR <fld> <op> <val>]", "List records (optionally filtered).", true},
        {"FIND",       "FIND <value>",                         "Find value in current order (when wired).", true},
        {"SEEK",       "SEEK <value>",                         "Seek exact match in index (when wired).", true},
        {"GOTO",       "GOTO <recno>",                         "Go to record number.", true},
        {"TOP",        "TOP",                                  "Go to first record.", true},
        {"BOTTOM",     "BOTTOM",                               "Go to last record.", true},
        {"APPEND",     "APPEND",                               "Append a new record (interactive in future).", true},
        {"APPEND BLANK","APPEND BLANK",                        "Append blank record.", true},
        {"REPLACE",    "REPLACE <field> WITH <value>",         "Replace field value in current record.", true},
        {"DELETE",     "DELETE [FOR <expr>]",                  "Mark record(s) deleted.", true},
        {"RECALL",     "RECALL [FOR <expr>]",                  "Un-delete record(s).", true},
        {"PACK",       "PACK",                                 "Permanently remove deleted rows (see notes).", true},
        {"EXPORT",     "EXPORT <csv>",                         "Export to CSV.", true},
        {"IMPORT",     "IMPORT <csv>",                         "Import from CSV.", true},
        {"STRUCT",     "STRUCT",                               "Display table structure.", true},
        {"STATUS",     "STATUS",                               "Display area status.", true},
        {"COUNT",      "COUNT [FOR <expr>]",                   "Count matching records.", true},

        // Stubs / common FoxPro commands not yet implemented
        {"SET ORDER",  "SET ORDER TO <tag>",                   "Set controlling index tag.", false},
        {"REINDEX",    "REINDEX",                              "Rebuild indexes.", false},
        {"SET RELATION","SET RELATION TO <expr> INTO <table>", "Relate tables.", false},
        {"ZAP",        "ZAP",                                  "Remove all records.", false},
        {"BROWSE",     "BROWSE",                               "Table browser.", false},
    };
    return k;
}

// Find exact name (case-insensitive)
const Item* find(std::string_view name);

// Return items whose name or syntax contains the token (case-insensitive)
std::vector<const Item*> search(std::string_view token);

} // namespace foxref

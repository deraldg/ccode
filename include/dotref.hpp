//dotref.hpp

#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace dotref {

struct Item {
    const char* name;    // canonical upper-case command name
    const char* syntax;  // short syntax line
    const char* summary; // one-liner (or longer help text)
    bool supported;      // whether DotTalk++ currently supports it
};

inline const std::vector<Item>& catalog() {
    static const std::vector<Item> k = {
        {"DBAREA",    "DBAREA", "Report or inspect the current DbArea/workspace (diagnostics).", true},

        {"STATUS",    "STATUS", "Display area status.", true},

        {"STRUCT",    "STRUCT", "Display table structure.", true},

        {"FIELDS",    "FIELDS", "Show structure (field list).", true},

        {"SET ORDER", "SET ORDER TO <tag>",
                 "Set controlling index tag (compatibility form; see SETORDER).", true},

        {"SETORDER",  "SETORDER <tag>|PHYSICAL",
                 "Set controlling order/tag for the current area (or PHYSICAL for natural order).", true},

        {"SET INDEX", "SET INDEX TO <base>", "Attach/activate a single-tag INX index (base name).", true},

        {"SETINDEX",  "SETINDEX <tag|path>", "Activate an index for the area (DotTalk++ convenience wiring).", true},

        {"REINDEX",   "REINDEX [ALL]",
                 "Rebuild index files for the current table (or all open tables).", true},

        {"CNX",       "CNX <name>",
                 "Index container command (CNX multi-tag support).", true},

        {"SETCNX",    "SETCNX TO <file>",
                 "Attach/activate a CNX container for the current area.", true},

        {"INDEXSEEK", "INDEXSEEK <value>", "Seek using the active index/order (DotTalk++ seek wiring).", true},

        {"WHERE",     "WHERE <expr>", "Apply an SQL-like filter to the current context.", true},

        {"SETFILTER", "SETFILTER <expr>", "Set a filter expression (DotTalk++ variant).", true},

        {"CLEAR",     "CLEAR [FILTER|ORDER|REL|ALL]", "Clear current state (filters/orders/relations) depending on context.", true},

        {"SETCASE",   "SETCASE ON|OFF", "Control case-sensitivity / collation mode for comparisons.", true},

        {"PREDHELP",  "PREDHELP [<topic>]", "Help for predicates/expressions and filtering.", true},

        {"PREDICATES","PREDICATES", "List supported predicates/operators for filtering.", true},

        {"SMARTLIST", "SMARTLIST [ALL|DELETED|<n>] [DEBUG] [FOR <expr>]",
        R"(LIST-style output with SQL-grade filtering (order-aware).

        Examples:
            SMARTLIST 20 FOR gpa >= 3.5
            SMARTLIST ALL FOR lname LIKE "SMI%"

        Notes:
            Evaluates filters using the expression/predicate pipeline.)", true},

        {"TABLE", "TABLE <subcommand> ...",
        R"(Table buffering / stale tracking.

        Subcommands:
            TABLE ON
                Enable table buffering for the current area. REPLACE writes into a per-field buffer and
                marks fields STALE instead of immediately writing to disk.

            TABLE OFF
                Disable buffering. REPLACE writes directly to disk (no buffering).

            TABLE STALE
                Show which fields are currently stale (buffered) for the current area.

            TABLE FRESH [ALL|<n>|<n>,<m>]
                Apply buffered updates to disk and clear stale flags on success.
                (Scope forms depend on build; ALL / area lists were previously supported.)

        Notes:
            - In current shakedown observations, LIST output reflects the physical record, not buffered overlays.
            - Some builds may not recognize TABLE REFRESH; prefer TABLE FRESH.)", true},

        {"COMMIT", "COMMIT",
        R"(Commit buffered TABLE updates to disk.

        Notes (current shakedown observations):
            - Clears TABLE stale state on success.
            - May currently trigger full INX rebuild work as part of the commit path (performance issue).)", true},

        {"ROLLBACK", "ROLLBACK",
                 "Discard buffered TABLE updates (planned; not confirmed in current shakedown runs).", false},

        {"EXPORT", "EXPORT <csv>", "Export to CSV.", true},

        {"IMPORT", "IMPORT <csv>", "Import from CSV.", true},

        {"SETPATH", "SETPATH | SETPATH RESET | SETPATH <SLOT> <path>",
        R"(Manage runtime root directories.

        Usage:
            SETPATH
                Show current roots.

            SETPATH RESET
                Restore defaults (based on DATA root).

            SETPATH <SLOT> <path>
                Set a root slot to absolute(path). If <path> is relative, it is resolved under DATA.

        Slots:
            DATA, DBF, INDEXES, SCHEMAS, SCRIPTS, TESTS, HELP, LOGS, TMP

        Notes:
            Used so HELP/TESTS/etc. can be moved without requiring full paths.)", true},

        {"SCHEMAS", "SCHEMAS OPEN [DBF|<dir>] | SCHEMAS CLOSE | SCHEMAS",
        R"(Bulk-open DBFs into consecutive work areas.

        Usage:
            SCHEMAS
                Report current workspace contents (where implemented).

            SCHEMAS CLOSE
                Close all work areas and clear relations state.

            SCHEMAS OPEN DBF
                Scan the configured DBF root and open tables into areas 0..N.

            SCHEMAS OPEN <dir>
                Scan a specific directory and open DBFs into areas 0..N.

        Notes:
            Intended for dataset bring-up and browser sessions.)", true},

        {"REL", "REL <subcommand> ...",
        R"(Relations engine commands (native DotTalk++ relation graph).

Subcommands:
    REL LIST
    REL LIST ALL
    REL REFRESH
    REL ENUM [LIMIT <n>] <child1> [<child2> ...] TUPLE <tuple-expr>
    REL SAVE [path] | REL SAVE AS <dataset>
    REL LOAD [path] | REL LOAD AS <dataset>
    REL ADD <parent> <child> ON <field>[,<field>...]
    REL CLEAR <parent>|ALL

Examples:
    REL LIST
    REL ADD STUDENTS ENROLL ON SID
    REL ADD ENROLL CLASSES ON CLS_ID
    REL REFRESH

    REL ENUM LIMIT 10 ENROLL CLASSES TASSIGN TEACHERS TUPLE ;
        STUDENTS.SID,STUDENTS.LNAME,ENROLL.CLS_ID,CLASSES.CID,TEACHERS.LNAME

Conceptual Model:
    REL maintains a directed relation graph between work areas.
    REL ADD defines edges.
    REL REFRESH recalculates relation state.
    REL ENUM traverses the graph and emits tuple rows.

Traversal Behavior:
    REL ENUM performs depth-first traversal of the relation chain.
    Each successful path produces one tuple row.

    Example chain:
        STUDENTS -> ENROLL -> CLASSES -> TASSIGN -> TEACHERS

Matching:
    Current implementation uses same-name field equality:
        parent.FIELD == child.FIELD

Notes:
    REL is the native DotTalk++ relation system.
    FoxPro-style SET RELATION commands map into this backend.

    REL ENUM is the row enumeration engine used by relational
    browsers and tuple views.)", true},

        {"REL ENUM", "REL ENUM [LIMIT <n>] <path...> TUPLE <tuple-expr>",
        R"(Traverse a relation chain and emit tuple rows.

Syntax:
    REL ENUM [LIMIT <n>] <child1> [<child2> ...] TUPLE <tuple-expr>

Example:
    REL ENUM LIMIT 5 ENROLL CLASSES TASSIGN TEACHERS
        TUPLE STUDENTS.SID,STUDENTS.LNAME,TEACHERS.LNAME

Execution Model:
    1. Current selected area is the root.
    2. Each child token represents the next relation edge.
    3. Traversal expands matches depth-first.

Example traversal:
    STUDENTS
        -> ENROLL
            -> CLASSES
                -> TASSIGN
                    -> TEACHERS

Each complete path emits one tuple row.

LIMIT:
    LIMIT applies to emitted rows, not intermediate matches.

Projection:
    The TUPLE clause defines which fields are emitted.

Used By:
    SIMPLEBROWSER
    SMARTBROWSER
    relational tuple views.)", true},

        {"TUPLE", "TUPLE <spec>",
        R"(Build one tuple row from fields across work areas.

        Examples:
            TUPLE *                    (all fields from current area)
            TUPLE LNAME,FNAME          (selected fields from current area)
            TUPLE #11.*                (all fields from area 11)
            TUPLE #9.*,#11.LNAME       (mix of areas)

        Notes:
            #n prefixes a work area slot; "*" means all fields for that area.
            Output fields are separated by ASCII Unit Separator (0x1F).)", true},

        {"TUPTALK", "TUPTALK", "DotTalk++ tuple/logical-row command.", true},

        {"FOXTALK", "FOXTALK",
        R"(Launch the Turbo Vision (FoxPro-style) TUI shell.

        Example:
            FOXTALK

        Notes:
            Intended for keyboard-driven browsing and diagnostics.
            Exits back to the DotTalk++ CLI.)", true},

        {"BROWSETUI","BROWSETUI", "Text-mode browser UI (developer tool).", true},

        {"BROWSETV", "BROWSETV", "Turbo Vision browser UI (developer tool).", true},

        {"BROWSER",  "BROWSER", "Developer browser command (experimental).", true},

        {"RECORDVIEW","RECORDVIEW", "Vertical record viewer.", true},

        {"SIMPLEBROWSER", "SIMPLEBROWSER [options]",
        R"(SIMPLEBROWSER  (SB)
        NAME
            SIMPLEBROWSER

        ALIASES
            SB

        PURPOSE
            Workspace/DbArea-centric interactive browser for table relationships and logical rows.

        SYNTAX
            SIMPLEBROWSER [FOR <expr>] [ORDER <tag>|PHYSICAL] [LIMIT <n>]

        OPTIONS / SUBCOMMANDS
            FOR <expr>           Apply a filter expression (tuple/expr aware).
            CLEAR FOR            Clear active filter.
            ORDER PHYSICAL|INX|CNX|<tag>   Change controlling order/tag.
            SHOW CHILDREN [LIMIT n]        Browse related child rows from current record.
            SHOW|HIDE SCHEMA|JSON          Toggle schema/JSON debug overlays.
            STATUS VERBOSE|COMPACT         Toggle status verbosity.

        EXAMPLES
            SIMPLEBROWSER
            SB FOR lname = "GRIMWOOD"
            SB ORDER LNAME
            SB SHOW CHILDREN LIMIT 20

        NOTES
            Simple Browser is designed for browsing within the current workspace DbArea.
            It is order-aware and expression/predicate aware.

        SEE ALSO
            SMARTBROWSER, SMARTLIST, REL, TUPLE, SCHEMAS

        STATUS
            stable
        )", true},

        {"SB", "SB", "Alias for SIMPLEBROWSER.", true},

        {"SMARTBROWSER", "SMARTBROWSER [options]",
        R"(SMARTBROWSER  (SM, SMART)
        NAME
            SMARTBROWSER

        ALIASES
            SM, SMART

        PURPOSE
            Advanced browser/workhorse for relational + expression-aware browsing across backends.

        SYNTAX
            SMARTBROWSER [<source>] [FOR <expr>] [ORDER <tag>|PHYSICAL]

        OPTIONS / SUBCOMMANDS
            <source>             Optional source (DbArea/workspace, SQLite3, or schema engine where available).
            FOR <expr>           Apply an expression filter using the predicate pipeline.
            ORDER <tag>|PHYSICAL Set controlling order/tag when browsing DbArea-backed sources.

        EXAMPLES
            SMARTBROWSER
            SM FOR gpa >= 3.5
            SMART ORDER LNAME

        NOTES
            Smart Browser is the flagship browser. It can operate with DbArea/workspaces and,
            when configured, can also operate with SQLite3 and schema-driven sources.

        SEE ALSO
            SIMPLEBROWSER, SMARTLIST, SCHEMAS, SQLITE, REL, TUPLE

        STATUS
            stable
        )", true},

        {"SMART", "SMART", "Alias for SMARTBROWSER.", true},

        {"SM",    "SM",    "Alias for SMARTBROWSER.", true},

        {"COLOR", "COLOR <GREEN|AMBER|DEFAULT>",
        R"(Switch CLI/TUI color theme.

        Examples:
            COLOR GREEN
            COLOR G+
            COLOR AMBER
            COLOR DEFAULT

        Notes:
            Used by FOXTALK/Turbo Vision palette wiring.)", true},

        {"TVISION",   "TVISION", "Turbo Vision diagnostics / demos.", true},

        {"TURBOPACK", "TURBOPACK", "Turbo Vision / pack-related utility.", true},

        {"FOXPRO",    "FOXPRO", "DotTalk++ UI / browser command.", true},

        {"HELP", "HELP [<topic>] [/FOX] [/PRED]",
                 "General help entry point (supports /FOX and /PRED views).", true},

        {"FOXHELP", "FOXHELP [<term>]",
        R"(List or search command help topics.

        Examples:
            FOXHELP
            FOXHELP SCHEMAS
            FOXHELP REL

        Notes:
            Uses the help catalogs and any HELP DBFs.)", true},

        {"CMDHELP", "CMDHELP [BUILD]",
        R"(Build / report the HELP command catalogs.

        Examples:
            CMDHELP BUILD
            CH               (alias report, if enabled)

        Notes:
            Mines command registry + foxref + optional DBF help sources.)", true},

        {"COMMANDSHELP", "COMMANDSHELP", "Command help (alias of CMDHELP).", true},

        {"CMDHELPCHK",   "CMDHELPCHK", "Validate HELP catalogs vs the command registry.", true},

        {"CMDARGCHK",    "CMDARGCHK", "Validate command argument parsing and report issues.", true},

        {"WSREPORT",     "WSREPORT", "Workspace report / diagnostics.", true},

        {"SHOW",         "SHOW <topic>", "Show internal engine state / debug views.", true},

        {"DUMP",         "DUMP", "Debug: print a structured dump of an internal value/state.", true},

        {"VERSION",      "VERSION", "Print build/version information.", true},

        {"DOTSCRIPT", "DOTSCRIPT <file.dts>",
        R"(Run a DotTalk script file (test harness / automation).

        Example:
            DOTSCRIPT run_all_sql_tests.dts)", true},

        {"TEST", "TEST <script>|<suite>", "DotTalk++ scripting/test harness command.", true},

        {"GO",   "GO <commandLine>", "Execute a DotTalk++ command line (dispatch to command handlers).", true},

        {"SCAN",    "SCAN [ALL|DELETED] [FOR <pred>] [WHILE <pred>] [NEXT <n>|REST]",
                 "Iterate records using a SCAN block (record loop).", true},

        {"ENDSCAN", "ENDSCAN", "End a SCAN block.", true},

        {"CONTINUE","CONTINUE", "Continue a running loop/scan block (scripting control flow).", true},

        {"LOOP",    "LOOP FOR <n> TIMES", "Begin a LOOP block (scripting).", true},

        {"ENDLOOP", "ENDLOOP", "End a LOOP block (scripting).", true},

        {"WHILE",   "WHILE <expr>", "Begin a WHILE block (scripting).", true},

        {"ENDWHILE","ENDWHILE", "End a WHILE block (scripting control flow).", true},

        {"UNTIL",   "UNTIL <expr>", "Begin an UNTIL block (scripting).", true},

        {"ENDUNTIL","ENDUNTIL", "End an UNTIL block (scripting).", true},

        {"IF",    "IF <expr>", "Conditional execution block: IF ... [ELSE] ... ENDIF.", false},

        {"ELSE",  "ELSE", "Optional ELSE branch for an IF block.", false},

        {"ENDIF", "ENDIF", "End an IF block.", false},

        {"SET VAR",  "SET VAR <name> TO <value> | SET VAR <name>",
                 "Set or show a macro variable (used by &name expansion in scripts).", true},

        {"SET VAR!", "SET VAR! <name> TO <value> | SET VAR! <name>",
                 "Force-set a macro variable (variant used in some scripts).", true},

        {"CALC", "CALC <expr>",
        R"(Evaluate an expression and display the result.

        Important parsing note:
            In current DotTalk++ behavior, if the expression contains operators like =, >=, <=,
            it should be wrapped in parentheses to avoid being parsed as CALCWRITE syntax.

        Examples:
            CALC 1 + 2
            CALC (UPPER("a") = "A")     && safe
            CALC UPPER("a") = "A"       && may be parsed as CALCWRITE

        Notes:
            Quoted strings containing '=' are treated as literals and should not trigger CALCWRITE.)", true},

        {"CALCWRITE", "CALCWRITE <expr>",
                 "Evaluate an expression and write the result to a target (see command usage).", true},

        {"EVAL", "EVAL <expr>", "Evaluate an expression and print its value.", true},

        {"CONCAT", "CONCAT(<c1>[, <c2> ...]) | CONCAT <args...>",
                 "Concatenate string arguments. (Available as CALC function; also usable as a command where wired.)", true},

        {"SQLITE", "SQLITE <subcommand> ...",
        R"(SQLite integration.

        Examples:
            SQLITE DB data\sql_regression.sqlite
            SQLITE EXEC CREATE TABLE t(x INT)
            SQLITE EXEC INSERT INTO t(x) VALUES (1)
            SQLITE SELECT * FROM t

        Notes:
            Used for regression testing and DBF↔SQL bridging experiments.)", true},

        {"SQLVER", "SQLVER", "Report SQLite availability and version.", true},

        {"SQL",    "SQL <statement>", "Execute an SQL statement using the configured SQL engine.", true},

        {"SQLSEL", "SQLSEL <expr>", "Execute an SQL SELECT and display results.", true},

        {"INSERT", "INSERT <statement>", "Insert data rows (scripting/SQL helper; see command usage).", true},

        {"UPDATE", "UPDATE <statement>", "Update data rows (scripting/SQL helper; see command usage).", true},

        {"SCHEMA", "SCHEMA [args...]", "Schema architect / schema engine command family.", true},

        {"VALIDATE", "VALIDATE <path>", "Schema/sidecar validation command.", true},

        {"AREA51", "AREA51", "Developer sandbox / experimental command.", true},

        {"GENERIC", "GENERIC", "Developer utility placeholder command.", true},

        {"NORMALIZE", "NORMALIZE <expr>", "Normalize/clean an input expression or text (developer utility).", true},

        {"RECORD", "RECORD", "Display or return the current record context (utility).", true},

        {"LOCK", "LOCK [RECORD|TABLE|<recno>...]", "Lock the current record (or table) for editing.", true},

        {"UNLOCK", "UNLOCK [ALL]", "Release a previously acquired lock (optionally all locks).", true},

        {"REFRESH", "REFRESH", "Refresh internal state / UI view (where applicable).", true},

        {"MULTIREP", "MULTIREP <field>=<expr> ...", "Perform multiple replace operations in one pass (text transform).", true},

        {"ASCEND", "ASCEND", "Set ascending sort direction for the active order/tag.", true},

        {"DESCEND", "DESCEND", "Set descending sort direction for the active order/tag.", true},
    };
    return k;
}

inline std::string upcopy(std::string_view s) {
    std::string out(s.begin(), s.end());
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

inline const Item* find(std::string_view name) {
    const std::string key = upcopy(name);
    for (const auto& item : catalog()) {
        if (upcopy(item.name) == key) {
            return &item;
        }
    }
    return nullptr;
}

inline std::vector<const Item*> search(std::string_view token) {
    std::vector<const Item*> matches;
    const std::string key = upcopy(token);

    for (const auto& item : catalog()) {
        const std::string n = upcopy(item.name);
        const std::string s = item.syntax  ? upcopy(item.syntax)  : std::string{};
        const std::string d = item.summary ? upcopy(item.summary) : std::string{};

        if (n.find(key) != std::string::npos ||
            s.find(key) != std::string::npos ||
            d.find(key) != std::string::npos) {
            matches.push_back(&item);
        }
    }
    return matches;
}

} // namespace dotref
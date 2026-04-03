// foxref.hpp
#pragma once
#include <string>
#include <vector>
#include <string_view>

namespace foxref {

struct Item {
    const char* name;    // canonical upper-case command name
    const char* syntax;  // short syntax line
    const char* summary; // one-liner (or longer help text)
    bool supported;      // whether DotTalk++ currently supports it
};

enum class BetaStatus : unsigned char {
    OPEN = 0,
    DONE,
    DEFERRED
};

struct BetaItem {
    const char* id;       // stable key, e.g., "BETA-3.1"
    const char* area;     // grouping, e.g., "Schema", "Tuple", "Build"
    const char* summary;  // short line
    const char* details;  // optional multi-line detail (may be nullptr)
    BetaStatus  status;   // OPEN/DONE/DEFERRED (compile-time defaults)
};

// Minimal seed set; extend anytime.
inline const std::vector<Item>& catalog() {
    static const std::vector<Item> k = {
        {"USE",       "USE <table> [IN <n>]", "Open a DBF in a work area.", true},

        {"CLOSE",     "CLOSE [ALL] | CLOSE DATABASES",
                 "Close the current work area or close all open tables (FoxPro: CLOSE TABLES/DATABASES/ALL).", true},

        {"SELECT",    "SELECT <workarea|alias>", "Select the active work area by number or alias.", true},

        {"AREA",      "AREA", "Report or set the current work area slot.", true},

        {"DBAREA",    "DBAREA", "Report or inspect the current DbArea/workspace (diagnostics).", true},

        {"STATUS",    "STATUS", "Display area status.", true},

        {"STRUCT",    "STRUCT", "Display table structure.", true},

        {"FIELDS",    "FIELDS", "Show structure (field list).", true},

        {"TOP",       "TOP", "Go to first record.", true},

        {"BOTTOM",    "BOTTOM", "Go to last record.", true},

        {"GOTO",      "GOTO <recno>", "Go to record number.", true},

        {"SKIP",      "SKIP [<nRecords>]", "Move the record pointer forward/backward by <nRecords> (default 1).", true},

        {"RECNO",     "RECNO()", "Return the current record number.", true},

        {"INDEX",     "INDEX ON <expr> TAG <tag> [OF <cdx>] | INDEX ON <expr> TO <idx>",
                 "Create an index on an expression (FoxPro syntax; CDX via TAG/OF, IDX via TO).", true},

        {"SET", "SET <option> [args]",
        R"(Modern settings/index router.

        Public syntax:
            SET INDEX TO <table>.cdx
            SET ORDER TO TAG <tag> [IN <alias>]
            SET ORDER TO <tag>
            SET ORDER TO 0
            SET FILTER TO <expr>
            SET FILTER TO
            SET TALK ON|OFF
            SET ECHO ON|OFF
            SET DELETED ON|OFF
            SET CASE
            SET CASE ON|OFF
            SET UNIQUE
            SET UNIQUE FIELD <field> ON|OFF
            SET RELATION[S] <args...>
            SET PATH
            SET PATH RESET
            SET PATH <slot> <path>

        Output routing:
            SET CONSOLE ON|OFF
            SET PRINT ON|OFF
            SET PRINT TO <file>
            SET ALTERNATE ON|OFF
            SET ALTERNATE TO <file>

        Notes:
            This is the preferred public entry point.
            Older standalone forms such as SETINDEX / SETORDER / SETFILTER / SETCASE
            are compatibility or internal targets behind the routed SET command.)", true},

        {"SET INDEX", "SET INDEX TO <table>.cdx",
                 "Preferred public syntax for attaching the current area's CDX container.", true},

        {"SET ORDER", "SET ORDER TO TAG <tag> [IN <alias>] | SET ORDER TO <tag> | SET ORDER TO 0",
                 "Preferred public syntax for activating a tag or returning to physical order.", true},

        {"SETINDEX",  "SETINDEX <args...>",
                 "Compatibility/internal target behind routed SET INDEX.", true},

        {"SETORDER",  "SETORDER <args...>",
                 "Compatibility/internal target behind routed SET ORDER.", true},

        {"REINDEX",   "REINDEX [ALL]",
                 "Legacy INX rebuild path. Public CDX/LMDB workflow typically uses BUILDLMDB or COMMIT-triggered rebuilds.", true},

        {"CNX",       "CNX <name>",
                 "Index container command (CNX multi-tag support).", true},

        {"SETCNX",    "SETCNX TO <file>",
                 "Developer/transitional command for CNX container attachment.", true},

        {"SETCDX",    "SETCDX TO <file>",
                 "Developer/transitional command for direct CDX container attachment.", true},

        {"SETLMDB",   "SETLMDB <args...>",
                 "Developer/transitional LMDB utility command. Public index workflow prefers SET INDEX / SET ORDER / BUILDLMDB.", true},

        {"CDX",       "CDX [args...]",
                 "Developer/transitional CDX command family.", true},

        {"BUILDLMDB", "BUILDLMDB [<table>|<container>]",
                 "Rebuild CDX/LMDB tags for the current table/container.", true},

        {"LMDB",      "LMDB <subcommand> ...",
                 "Developer LMDB utility command family for direct backend inspection/testing.", true},

        {"LIST_LMDB", "LIST_LMDB [args...]",
                 "Developer listing helper for LMDB-backed order inspection.", true},

        {"FIND",      "FIND <cSearch>", "Find a value in the current order/index (FoxPro: FIND).", true},

        {"SEEK",      "SEEK <exp> [IN <alias>]", "Seek an exact match using the current index/order.", true},

        {"INDEXSEEK", "INDEXSEEK <value>", "Seek using the active index/order (DotTalk++ seek wiring).", true},

        {"LIST",      "LIST [ALL|DELETED] [FIELDS <fieldlist>] [FOR <pred>] [WHILE <pred>] [NEXT <n>|REST]",
                 "List records with optional field selection and filtering (default skips DELETED unless DELETED/ALL is specified).", true},

        {"DISPLAY",   "DISPLAY [FIELDS <fieldlist>]", "Display the current record (optionally selected fields).", true},

        {"COUNT",     "COUNT [ALL|DELETED] [FOR <pred>] [WHILE <pred>] [NEXT <n>|REST] [TO <memvar>]",
                 "Count matching records; optionally store the result to a variable.", true},

        {"LOCATE",    "LOCATE [FOR <pred>] [WHILE <pred>]", "Locate the first record matching a predicate/filter.", true},

        {"WHERE",     "WHERE <expr>", "Apply an SQL-like filter to the current context.", true},

        {"SET FILTER","SET FILTER TO <expr> | SET FILTER TO",
                 "Preferred public syntax for setting or clearing the current filter.", true},

        {"SETFILTER", "SETFILTER <args...>", "Compatibility/internal target behind routed SET FILTER.", true},

        {"CLEAR",     "CLEAR [FILTER|ORDER|REL|ALL]", "Clear current state (filters/orders/relations) depending on context.", true},

        {"SET CASE",  "SET CASE [ON|OFF]",
                 "Preferred public syntax for viewing or setting case-sensitive comparison mode.", true},

        {"SETCASE",   "SETCASE <args...>", "Compatibility/internal target behind routed SET CASE.", true},

        {"PREDHELP",  "PREDHELP [<topic>]", "Help for predicates/expressions and filtering.", true},

        {"PREDICATES","PREDICATES", "List supported predicates/operators for filtering.", true},

        {"SMARTLIST", "SMARTLIST [ALL|DELETED|<n>] [DEBUG] [FOR <expr>]",
        R"(LIST-style output with SQL-grade filtering (order-aware).

        Examples:
            SMARTLIST 20 FOR gpa >= 3.5
            SMARTLIST ALL FOR lname LIKE "SMI%"

        Notes:
            Evaluates filters using the expression/predicate pipeline.)", true},

        {"APPEND",       "APPEND", "Append a new record (interactive in future).", true},

        {"APPEND_BLANK", "APPEND_BLANK",
                 "Append a blank record to the current table/work area. (FoxPro equivalent: APPEND BLANK)", true},

        {"APPEND BLANK", "APPEND BLANK",
                 "Append blank record (compatibility alias).", false},

        {"REPLACE",   "REPLACE <field> WITH <expr> [FOR <pred>]",
                 "Replace a field value with an expression (optionally across records).", true},

        {"EDIT",      "EDIT <field>", "Edit a field/value interactively (where supported).", true},

        {"DELETE",    "DELETE [FOR <pred>]", "Mark record(s) deleted.", true},

        {"RECALL",    "RECALL [FOR <pred>]", "Un-delete record(s).", true},

        {"UNDELETE",  "UNDELETE", "Un-delete (recall) the current record or selection.", true},

        {"PACK",      "PACK", "Permanently remove deleted rows (see notes).", true},

        {"ZAP",       "ZAP", "Remove all records.", true},

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

        {"COMMIT", "COMMIT [ALL]",
        R"(Commit buffered TABLE updates to disk.

        Notes:
            - Applies buffered TABLE writes.
            - May rebuild active indexes automatically after success.
            - In current public CDX/LMDB workflow, COMMIT can trigger a CDX/LMDB rebuild when the area is stale and a CDX order is active.)", true},

        {"ROLLBACK", "ROLLBACK",
                 "Discard buffered TABLE updates (planned; not confirmed in current shakedown runs).", false},

        {"EXPORT", "EXPORT <csv>", "Export to CSV.", true},

        {"IMPORT", "IMPORT <csv>", "Import from CSV.", true},

        {"COPY",   "COPY FILE <source> TO <dest>",
                 "Copy a file to a new location (FoxPro: COPY FILE ... TO ...).", true},

        {"DIR",    "DIR [<pattern>]", "List directory contents.", true},

        {"ERASE",  "ERASE <filespec>", "Delete file(s) matching <filespec> (FoxPro: ERASE).", true},

        {"SET PATH", "SET PATH | SET PATH RESET | SET PATH <SLOT> <path>",
        R"(Manage runtime root directories.

        Usage:
            SET PATH
                Show current roots.

            SET PATH RESET
                Restore defaults (based on DATA root).

            SET PATH <SLOT> <path>
                Set a root slot to absolute(path). If <path> is relative, it is resolved under DATA.

        Slots:
            DATA, DBF, INDEXES, SCHEMAS, SCRIPTS, TESTS, HELP, LOGS, TMP

        Notes:
            Relative paths are resolved under DATA.
            Example:
                SET PATH INDEXES indexes
            not:
                SET PATH INDEXES data/indexes
            when DATA already points at the data root.)", true},

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
            Compatibility/internal command behind routed SET PATH.)", true},

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
        R"(Relations engine commands.

        Subcommands:
            REL LIST
            REL LIST ALL
            REL REFRESH
            REL ENUM [LIMIT n] <path...> TUPLE <projection>
            REL SAVE [path]
            REL SAVE AS <dataset>
            REL LOAD [path]
            REL LOAD AS <dataset>
            REL ADD <parent> <child> ON <field>
            REL CLEAR <parent>
            REL CLEAR ALL

        Examples:
            REL ADD STUDENTS ENROLL ON SID
            REL ADD ENROLL CLASSES ON CLS_ID
            REL LIST
            REL LIST ALL
            REL REFRESH

            REL ENUM LIMIT 10 ENROLL CLASSES TASSIGN TEACHERS TUPLE
                STUDENTS.SID,STUDENTS.LNAME,ENROLL.CLS_ID,CLASSES.CID,TEACHERS.LNAME

        Notes:
            REL is the modern DotTalk++ relationship engine.
            It stores and traverses parent/child links explicitly.
            FoxPro-style SET RELATION syntax is now parsed and translated into this backend.
            REL ENUM is the traversal/projection side of the engine and should be documented further as it stabilizes.)", true},

        {"SET RELATION", "SET RELATION TO <expr> INTO <child> [,...] | SET RELATION ADDITIVE TO <expr> INTO <child> | SET RELATION OFF INTO <child> | SET RELATION OFF ALL",
        R"(FoxPro-style routed relation syntax.

        Supported forms:
            SET RELATION TO SID INTO ENROLL
            SET RELATION TO SID INTO ENROLL, SID INTO STUD_MAJ
            SET RELATION ADDITIVE TO SID INTO STUD_MAJ
            SET RELATION OFF INTO ENROLL
            SET RELATION OFF ALL

        Examples:
            SELECT STUDENTS
            SET RELATION TO SID INTO ENROLL

            SELECT STUDENTS
            SET RELATION TO SID INTO ENROLL, SID INTO STUD_MAJ

            SELECT STUDENTS
            SET RELATION TO SID INTO ENROLL
            SET RELATION ADDITIVE TO SID INTO STUD_MAJ

            SELECT STUDENTS
            SET RELATION OFF INTO ENROLL

            SELECT STUDENTS
            SET RELATION OFF ALL

        Notes:
            DotTalk++ parses FoxPro SET RELATION syntax and maps it to the modern REL backend.
            Non-ADDITIVE SET RELATION replaces the current parent's outgoing relations.
            ADDITIVE appends another child relation for the same parent.
            OFF INTO removes one child relation from the current parent.
            OFF ALL clears all outgoing relations for the current parent.)", true},

        {"RELATIONS", "SET RELATION ...",
        R"(Compatibility topic for FoxPro-style relation wiring.

        See:
            SET RELATION
            REL
            REL LIST
            REL ENUM

        Examples:
            SET RELATION TO SID INTO ENROLL
            SET RELATION ADDITIVE TO SID INTO STUD_MAJ
            SET RELATION OFF ALL

        Notes:
            In DotTalk++, FoxPro relation syntax is implemented by parsing SET RELATION
            and translating it into the REL engine's internal relationship model.)", true},

        {"REL_LIST", "REL_LIST", "Compatibility alias for REL LIST.", true},

        {"REL_REFRESH", "REL_REFRESH", "Compatibility alias for REL REFRESH.", true},

        {"CMDREL", "CMDREL [LIST|REFRESH]", "Relations help / compatibility command.", true},

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

        {"BROWSE",  "BROWSE", "Table browser (FoxPro).", false},

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
            ORDER PHYSICAL|INX|CNX|CDX|<tag>   Change controlling order/tag.
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

        {"INIT",         "INIT", "Initialize/report runtime path roots and environment.", true},

        {"DBAREAS",      "DBAREAS", "Report all open DbAreas/workspaces.", true},

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

        {"FORMULA", "? <expr> | ?? <expr>", "Evaluate/print an expression (FoxPro-style ? / ?? shortcut).", true},

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

        {"ALLTRIM",  "ALLTRIM(<cExpression>)", "Remove leading and trailing spaces from <cExpression>.", true},

        {"TRIM",     "TRIM(<cExpression>)", "Remove trailing spaces from <cExpression> (alias/compat; see RTRIM).", true},

        {"LTRIM",    "LTRIM(<cExpression>)", "Remove leading spaces from <cExpression>.", true},

        {"RTRIM",    "RTRIM(<cExpression>)", "Remove trailing spaces from <cExpression>.", true},

        {"UPPER",    "UPPER(<cExpression>)", "Convert <cExpression> to upper-case.", true},

        {"LOWER",    "LOWER(<cExpression>)", "Convert <cExpression> to lower-case.", true},

        {"LEFT",     "LEFT(<cExpression>, <nChars>)", "Return the left-most <nChars> characters of <cExpression>.", true},

        {"RIGHT",    "RIGHT(<cExpression>, <nChars>)", "Return the right-most <nChars> characters of <cExpression>.", true},

        {"SUBSTR",   "SUBSTR(<cExpr>, <nStart>[, <nLen>])",
                 "Return substring of <cExpr> starting at <nStart> for <nLen> characters (or to end).", true},

        {"LEN",      "LEN(<cExpression>)", "Return the length (character count) of <cExpression>.", true},

        {"AT",       "AT(<cSearch>, <cExpression>[, <nOccur>])",
                 "Return the 1-based position of <cSearch> within <cExpression> (case-sensitive).", true},

        {"ATC",      "ATC(<cSearch>, <cExpression>[, <nOccur>])",
                 "Return the 1-based position of <cSearch> within <cExpression> (case-insensitive).", true},

        {"STUFF",    "STUFF(<cExpr>, <nStart>, <nLen>, <cRepl>)",
                 "Replace <nLen> characters of <cExpr> starting at <nStart> with <cRepl>.", true},

        {"CHR",      "CHR(<nAsciiCode>)", "Convert an integer ASCII code (0-255) to a 1-character string.", true},

        {"ASC",      "ASC(<cExpression>)", "Return the ASCII code (0-255) of the first character of <cExpression>.", true},

        {"SPACE",    "SPACE(<nSpaces>)", "Return a character string containing <nSpaces> spaces.", true},

        {"REPLICATE","REPLICATE(<cExpression>, <nTimes>)", "Repeat <cExpression> <nTimes> times.", true},

        {"STR",      "STR(<nExpr>[, <nLen>[, <nDec>]])",
                 "Convert numeric <nExpr> to a character string (optionally controlling width/decimals).", true},

        {"VAL",      "VAL(<cExpression>)", "Convert a numeric-looking character expression to a number.", true},

        {"PADL",     "PADL(<cExpression>, <nLen>[, <cFill>])", "Left-pad to length <nLen>.", true},

        {"PADR",     "PADR(<cExpression>, <nLen>[, <cFill>])", "Right-pad to length <nLen>.", true},

        {"PADC",     "PADC(<cExpression>, <nLen>[, <cFill>])", "Center-pad to length <nLen>.", true},

        {"PROPER",   "PROPER(<cExpression>)", "Convert <cExpression> to Proper Case (title case).", true},

        {"TRANSFORM","TRANSFORM(<eExpr>[, <cPicture>])",
                 "Format <eExpr> as a string using an optional picture/mask.", true},

        {"DATE",      "DATE()", "Return the current system date.", true},

        {"TIME",      "TIME()", "Return the current system time as a character string (HH:MM:SS).", true},

        {"CTOD",      "CTOD(<cDate>)",
                 "Convert a character date to a Date value using current SET DATE/SET CENTURY.", true},

        {"DTOC",      "DTOC(<dDate>)",
                 "Convert a Date value to a character string using current SET DATE/SET CENTURY.", true},

        {"SUM", "SUM <expr> TO <memvar> [ALL|DELETED] [FOR <pred>] [WHILE <pred>] [NEXT <n>|REST]",
                 "Sum a numeric expression over a record set.", true},

        {"AVERAGE", "AVERAGE <expr> TO <memvar> [ALL|DELETED] [FOR <pred>] [WHILE <pred>] [NEXT <n>|REST]",
                 "Compute the average of a numeric expression over a record set.", true},

        {"AVG", "AVG <expr> TO <memvar>", "Alias for AVERAGE (xBase compatibility).", true},

        {"MIN", "MIN(<e1>, <e2>)",
                 "Return the smaller of <e1> and <e2> (aggregate MIN typically via SQL/CALC).", true},

        {"MAX", "MAX(<e1>, <e2>)",
                 "Return the larger of <e1> and <e2> (aggregate MAX typically via SQL/CALC).", true},

        {"ECHO",  "ECHO <text>", "Print text to the console.", true},

        {"!",     "! <shell-command>", "Alias for BANG (execute an OS command).", true},

        {"BANG",  "BANG <expr>", "Execute an OS command (shell escape).", true},

        {"RUN",   "RUN /N <command> | RUN <file>", "Run an OS command (FoxPro).", false},

        {"DO",    "DO <program> [WITH <args>]", "Execute a program. (Use DOTSCRIPT instead.)", false},

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

// ----------------------- Beta checklist catalog ------------------------------

// compile-time catalog
const std::vector<BetaItem>& beta_catalog();
const BetaItem* beta_find(std::string_view id);
std::vector<const BetaItem*> beta_by_area(std::string_view area);

// runtime override support
BetaStatus beta_effective_status(const BetaItem& it);
void beta_set_status(std::string_view id, BetaStatus st);
void beta_clear_status(std::string_view id);
void beta_clear_all_status_overrides();

// persistence
bool beta_save_overrides(std::string* err = nullptr);
bool beta_load_overrides(std::string* err = nullptr);
std::string beta_default_status_path();

// item catalog lookup
const Item* find(std::string_view name);
std::vector<const Item*> search(std::string_view token);

} // namespace foxref
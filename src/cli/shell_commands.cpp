// src/cli/shell_commands.cpp
// DotTalk++: Extracted command registry (was previously embedded in shell.cpp)

#include "shell_commands.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>

#include "textio.hpp"
#include "shell_api.hpp"               // shell_dispatch_line() decl
#include "command_registry.hpp"        // dli::registry()
#include "set_relations.hpp"           // relations_api::refresh_if_enabled()
#include "cli/order_state.hpp"         // orderstate helpers
#include "cli/dirty_prompt.hpp"        // dirty prompt wrappers

#include "xbase.hpp"

using xbase::DbArea;

namespace {

static inline std::string basename_upper(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    auto slash = path.find_last_of('/');
    if (slash != std::string::npos) path.erase(0, slash + 1);
    auto dot = path.find_last_of('.');
    if (dot != std::string::npos) path.erase(dot);
    std::transform(path.begin(), path.end(), path.begin(),
                   [](unsigned char c) { return char(std::toupper(c)); });
    return path;
}

static inline bool is_digits(const std::string& s) {
    if (s.empty()) return false;
    for (unsigned char c : s) if (!std::isdigit(c)) return false;
    return true;
}

static int resolve_area_index_by_name(xbase::XBaseEngine& eng, const std::string& tokRaw) {
    std::string tok = textio::trim(tokRaw);
    if (tok.empty()) return -1;

    if (is_digits(tok)) {
        int n = 0;
        try { n = std::stoi(tok); }
        catch (...) { return -1; }
        if (n >= 0 && n < xbase::MAX_AREA) return n;
        return -1;
    }

    const std::string want = textio::up(tok);
    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        xbase::DbArea& A = eng.area(i);
        if (!A.isOpen()) continue;
        const std::string base = basename_upper(A.name());
        if (base == want) return i;
    }
    return -1;
}

} // namespace

extern "C" void register_shell_commands(xbase::XBaseEngine& eng, bool include_ui_cmds)
{
    using namespace dli;

    // With engine cursor hook active, avoid manual refresh on cursor-moving commands.
    // Keep manual refresh for: open/close/select, relation-definition changes, and data mutations.

    registry().add("USE",   [](DbArea& A, std::istringstream& S){
        if (!dottalk::dirty::maybe_prompt_area(A, "USE")) {
            std::cout << "USE canceled.\n";
            return;
        }
        cmd_USE(A,S);
        relations_api::refresh_if_enabled();
    });

    registry().add("VUSE",   [](DbArea& A, std::istringstream& S){
        if (!dottalk::dirty::maybe_prompt_area(A, "USE")) {
            std::cout << "USE canceled.\n";
            return;
        }
        cmd_USE(A,S);
        relations_api::refresh_if_enabled();
    });


    registry().add("CLOSE", [](DbArea& A, std::istringstream& S){
        if (!dottalk::dirty::maybe_prompt_area(A, "CLOSE")) {
            std::cout << "CLOSE canceled.\n";
            return;
        }
        cmd_CLOSE(A,S);
        relations_api::refresh_if_enabled();
    });

    registry().add("SELECT",  [&](DbArea& /*A*/, std::istringstream& S){
        std::string tok;
        if (!(S >> tok)) {
            std::cout << "Usage: SELECT <0.." << (xbase::MAX_AREA-1) << " | <name>>\n";
            return;
        }
        int idx = resolve_area_index_by_name(eng, tok);
        if (idx < 0) {
            std::cout << "Usage: SELECT <0.." << (xbase::MAX_AREA-1) << " | <name>>\n";
            return;
        }
        eng.selectArea(idx);
        std::cout << "Selected area " << idx << ".\n";
        DbArea& cur = eng.area(eng.currentArea());
        std::cout << "Current area: " << eng.currentArea() << "\n";
        if (cur.isOpen()) {
            std::cout << "  File: " << cur.name()
                      << "  Recs: " << cur.recCount()
                      << "  Recno: " << cur.recno() << "\n";
        } else {
            std::cout << "  (no file open)\n";
        }
        relations_api::refresh_if_enabled();
    });

    registry().add("AREA51",    [&](DbArea&, std::istringstream&){
        int i = eng.currentArea();
        DbArea& cur = eng.area(i);
        std::cout << "Current area: " << i << "\n";
        if (cur.isOpen()) {
            std::cout << "  File: " << cur.name()
                      << "  Recs: " << cur.recCount()
                      << "  Recno: " << cur.recno() << "\n";
            try {
                bool asc = orderstate::isAscending(cur);
                std::string idx = orderstate::hasOrder(cur) ? orderstate::orderName(cur) : std::string("(none)");
                std::string tag = orderstate::hasOrder(cur) ? orderstate::activeTag(cur) : std::string("(none)");
                std::cout << "  Order: " << (asc ? "ASCEND" : "DESCEND") << "\n"
                          << "  Index file  : " << idx << "\n"
                          << "  Active tag  : " << tag << "\n";
            } catch (...) {}
        } else {
            std::cout << "  (no file open)\n";
        }
    });

    // Cursor movers: rely on engine cursor hook (no manual refresh here)
    registry().add("GO",     [](DbArea& A, std::istringstream& S){ cmd_GO(A,S);      });
    registry().add("TOP",    [](DbArea& A, std::istringstream& S){ cmd_TOP(A,S);     });
    registry().add("BOTTOM", [](DbArea& A, std::istringstream& S){ cmd_BOTTOM(A,S);  });
    registry().add("GOTO",   [](DbArea& A, std::istringstream& S){ cmd_GOTO(A,S);    });
    registry().add("SKIP",   [](DbArea& A, std::istringstream& S){ cmd_SKIP(A,S);    });

    // Non-positioning (should preserve cursor): no refresh here
    registry().add("COUNT",    [](DbArea& A, std::istringstream& S){ cmd_COUNT(A,S);    });
    registry().add("LIST",     [](DbArea& A, std::istringstream& S){ cmd_LIST(A,S);     });
    registry().add("LIST_LMDB",[](DbArea& A, std::istringstream& S){ cmd_LIST_LMDB(A,S);});
    registry().add("DISPLAY",  [](DbArea& A, std::istringstream& S){ cmd_DISPLAY(A,S);  });
    registry().add("GPS",      [](DbArea& A, std::istringstream& S){ cmd_GPS(A,S);      });

    // Interactive browsers typically move cursor internally; hook covers it.
    registry().add("SIMPLEBROWSER",[](DbArea& A, std::istringstream& S){ cmd_SIMPLE_BROWSER(A,S); });
    registry().add("BROWSE",       [](DbArea& A, std::istringstream& S){ cmd_BROWSE(A,S);         });
    registry().add("ERSATZ",       [](DbArea& A, std::istringstream& S){ cmd_RBROWSE(A,S);         });
    registry().add("BROWSER",      [](DbArea& A, std::istringstream& S){ cmd_BROWSER(A,S);        });
    registry().add("BROWSETUI",    [](DbArea& A, std::istringstream& S){ cmd_BROWSETUI(A,S);      });

    // TABLE buffering
    registry().add("TABLE",    [](DbArea& A, std::istringstream& S){ cmd_TABLE(A,S);    });
    registry().add("COMMIT",   [](DbArea& A, std::istringstream& S){ cmd_COMMIT(A,S);   });
    registry().add("ROLLBACK", [](DbArea& A, std::istringstream& S){ cmd_ROLLBACK(A,S); });

    // Filter changes affect relation results without moving cursor: keep manual refresh
    registry().add("SETFILTER", [](DbArea& A, std::istringstream& S){ cmd_SETFILTER(A,S); relations_api::refresh_if_enabled(); });

    registry().add("SMARTBROWSER", [](DbArea& A, std::istringstream& S){ cmd_SMART_BROWSER(A,S);  });

#if defined(DOTTALK_TV_AVAILABLE) && DOTTALK_TV_AVAILABLE
    if (include_ui_cmds) {
        registry().add("TVISION",    [](DbArea& A, std::istringstream& S){ cmd_TVISION(A,S); });
        registry().add("FOXPRO",     [](DbArea& A, std::istringstream& S){ cmd_FOXPRO(A,S); });
        registry().add("FOXTALK",    [](DbArea& A, std::istringstream& S){ cmd_FOXTALK(A,S); });
        registry().add("GENERIC",    [](DbArea& A, std::istringstream& S){ cmd_GENERIC(A,S); });
        registry().add("BROWSETV",   [](DbArea& A, std::istringstream& S){ cmd_BROWSETV(A,S); });
        registry().add("RECORD",     [](DbArea& A, std::istringstream& S){ cmd_RECORD(A,S); });
        registry().add("RECORDVIEW", [](DbArea& A, std::istringstream& S){ cmd_RECORDVIEW(A,S); });
    }
#else
    (void)include_ui_cmds;
#endif

    registry().add("COPY",         [](DbArea& A, std::istringstream& S){ cmd_COPY(A,S);   });
    registry().add("EXPORT",       [](DbArea& A, std::istringstream& S){ cmd_EXPORT(A,S); });
    registry().add("IMPORT",       [](DbArea& A, std::istringstream& S){ cmd_IMPORT(A,S); });

    // Data mutations: keep manual refresh (keys/visibility can change without cursor move)
    registry().add("APPEND",       [](DbArea& A, std::istringstream& S){ cmd_APPEND(A,S);       relations_api::refresh_if_enabled(); });
    registry().add("APPEND_BLANK", [](DbArea& A, std::istringstream& S){ cmd_APPEND_BLANK(A,S); relations_api::refresh_if_enabled(); });
    registry().add("DELETE",       [](DbArea& A, std::istringstream& S){ cmd_DELETE(A,S);       relations_api::refresh_if_enabled(); });
    registry().add("RECALL",       [](DbArea& A, std::istringstream& S){ cmd_RECALL(A,S);       relations_api::refresh_if_enabled(); });
    registry().add("UNDELETE",     [](DbArea& A, std::istringstream& S){ cmd_RECALL(A,S);       relations_api::refresh_if_enabled(); });
    registry().add("PACK",         [](DbArea& A, std::istringstream& S){ cmd_PACK(A,S);         relations_api::refresh_if_enabled(); });
    registry().add("TURBOPACK",    [](DbArea& A, std::istringstream& S){ cmd_PACK(A,S);         relations_api::refresh_if_enabled(); });

    registry().add("FIELDS",     [](DbArea& A, std::istringstream& S){ cmd_FIELDS(A,S);    });
    registry().add("FIELDMGR",   [](DbArea& A, std::istringstream& S){ cmd_FIELDMGR(A,S);  });

    // Cursor movers (seek/find/order): rely on cursor hook
    registry().add("FIND",       [](DbArea& A, std::istringstream& S){ cmd_FIND(A,S);     });
    registry().add("SEEK",       [](DbArea& A, std::istringstream& S){ cmd_SEEK(A,S);     });
    registry().add("SETORDER",   [](DbArea& A, std::istringstream& S){ cmd_SETORDER(A,S); });

    registry().add("SET",        [](DbArea& A, std::istringstream& S){ cmd_SET(A,S); });
    registry().add("SET UNIQUE", [](DbArea& A, std::istringstream& S){ cmd_SET_UNIQUE(A,S); });
    registry().add("SETPATH",    [](DbArea& A, std::istringstream& S){ cmd_SETPATH(A,S); });
    registry().add("VALIDATE",   [](DbArea& A, std::istringstream& S){ cmd_VALIDATE(A,S); });

#if DOTTALK_WITH_INDEX
    // Index ops may reposition internally; rely on cursor hook
    registry().add("CNX",       [](DbArea& A, std::istringstream& S){ cmd_CNX(A,S);      });
    registry().add("CDX",       [](DbArea& A, std::istringstream& S){ cmd_CDX(A,S);      });
    registry().add("SETCNX",    [](DbArea& A, std::istringstream& S){ cmd_SETCNX(A,S);   });
    registry().add("SETCDX",    [](DbArea& A, std::istringstream& S){ cmd_SETCDX(A,S);   });
    registry().add("INDEX",     [](DbArea& A, std::istringstream& S){ cmd_INDEX(A,S);    });
    registry().add("REINDEX",   [](DbArea& A, std::istringstream& S){ cmd_REINDEX(A,S);  });
    registry().add("REBUILD",   [](DbArea& A, std::istringstream& S){ cmd_REBUILD(A,S);  });
    registry().add("BUILDLMDB", [](DbArea& A, std::istringstream& S){ cmd_BUILDLMDB(A,S);});
    registry().add("LMDBDUMP",  [](DbArea& A, std::istringstream& S){ cmd_LMDB_DUMP(A,S);});
    registry().add("SETLMDB",   [](DbArea& A, std::istringstream& S){ cmd_SETLMDB(A,S);  });
    registry().add("SETINDEX",  [](DbArea& A, std::istringstream& S){ cmd_SETINDEX(A,S); });
    registry().add("ASCEND",    [](DbArea& A, std::istringstream& S){ cmd_ASCEND(A,S);   });
    registry().add("DESCEND",   [](DbArea& A, std::istringstream& S){ cmd_DESCEND(A,S);  });
    registry().add("INDEXSEEK", [](DbArea& A, std::istringstream& S){ cmd_INDEXSEEK(A,S);});
    registry().add("LMDB",      [](DbArea& A, std::istringstream& S){ cmd_LMDB(A,S);     });
    registry().add("LMDB_UTIL", [](DbArea& A, std::istringstream& S){ cmd_LMDB_UTIL(A,S);});
#endif 

    // Aggregate helpers
    registry().add("AGGS",    [](DbArea& A, std::istringstream& S){ cmd_AGGS(A,S); });
    registry().add("SUM",     [](DbArea& A, std::istringstream& S){ cmd_SUM(A,S);  });
    registry().add("AVG",     [](DbArea& A, std::istringstream& S){ cmd_AVG(A,S);  });
    registry().add("AVERAGE", [](DbArea& A, std::istringstream& S){ cmd_AVG(A,S);  });
    registry().add("MIN",     [](DbArea& A, std::istringstream& S){ cmd_MIN(A,S);  });
    registry().add("MAX",     [](DbArea& A, std::istringstream& S){ cmd_MAX(A,S);  });

#if DOTTALK_WITH_INDEX
    // Relation-definition changes: refresh explicitly
    registry().add("SET RELATION",[](DbArea& A, std::istringstream& S){ cmd_SET_RELATIONS(A,S); relations_api::refresh_if_enabled(); });
    registry().add("RELATIONS",   [](DbArea& A, std::istringstream& S){ cmd_RELATIONS_LIST(A,S); });
    registry().add("REL_LIST",    [](DbArea& A, std::istringstream& S){ cmd_RELATIONS_LIST(A,S); });
    registry().add("REL_REFRESH", [](DbArea& A, std::istringstream& S){ cmd_RELATIONS_REFRESH(A,S); });

    registry().add("TUPTALK", [](DbArea& A, std::istringstream& S){ cmd_TUPTALK(A,S); });
    registry().add("TUPLE",   [](DbArea& A, std::istringstream& S){ cmd_TUPLE(A,S);   });
#endif

    registry().add("MEMO",   [](DbArea& A, std::istringstream& S){ cmd_MEMO(A,S); });

    registry().add("REL",    [](DbArea& A, std::istringstream& S){ cmd_REL(A,S); });

    registry().add("LOCK",   [](DbArea& A, std::istringstream& S){ cmd_LOCK(A,S);   });
    registry().add("UNLOCK", [](DbArea& A, std::istringstream& S){ cmd_UNLOCK(A,S); });

    registry().add("CLEAR",  [](DbArea& A, std::istringstream& S){ cmd_CLEAR(A,S);  });
    registry().add("CREATE", [](DbArea& A, std::istringstream& S){ cmd_CREATE(A,S); });
    registry().add("ERASE",  [](DbArea& A, std::istringstream& S){ cmd_ERASE(A,S);  });

    registry().add("DUMP", [](DbArea& A, std::istringstream& S){ cmd_DUMP(A,S); });
    registry().add("EDIT", [](DbArea& A, std::istringstream& S){ cmd_EDIT(A,S); });

    // Cursor movers: rely on cursor hook
    registry().add("LOCATE",   [](DbArea& A, std::istringstream& S){ cmd_LOCATE(A,S);   });
    registry().add("CONTINUE", [](DbArea& A, std::istringstream& S){ cmd_CONTINUE(A,S); });

    registry().add("AREA",  [](DbArea& A, std::istringstream& S){ cmd_AREA(A,S);  });
    registry().add("RECNO", [](DbArea& A, std::istringstream& S){ cmd_RECNO(A,S); });

    // Explicit REFRESH: keep manual refresh (no cursor movement required)
    registry().add("REFRESH", [](DbArea& A, std::istringstream& S){ cmd_REFRESH(A,S); relations_api::refresh_if_enabled(); });

    // Data mutation: keep manual refresh
    registry().add("REPLACE",  [](DbArea& A, std::istringstream& S){ cmd_REPLACE(A,S);       relations_api::refresh_if_enabled(); });
    registry().add("MULTIREP", [](DbArea& A, std::istringstream& S){ cmd_REPLACE_MULTI(A,S); relations_api::refresh_if_enabled(); });

    registry().add("STATUS",  [](DbArea& A, std::istringstream& S){ cmd_STATUS(A,S); relations_api::refresh_if_enabled(); });

    registry().add("STRUCT",  [](DbArea& A, std::istringstream& S){ cmd_STRUCT(A,S);   });
    registry().add("SCHEMA",  [](DbArea& A, std::istringstream& S){ cmd_SCHEMA(A,S);   });
    registry().add("SCHEMAS", [](DbArea& A, std::istringstream& S){ cmd_SCHEMAS(A,S);  });
    registry().add("PROJECTS",[](DbArea& A, std::istringstream& S){ cmd_PROJECTS(A,S);  });
    registry().add("WSREPORT",[](DbArea& A, std::istringstream& S){ cmd_WSREPORT(A,S); });

    // Destructive: refresh explicitly (cursor may become invalid/zero)
    registry().add("ZAP", [](DbArea& A, std::istringstream& S){ cmd_ZAP(A,S); relations_api::refresh_if_enabled(); });

    registry().add("DIR",       [](DbArea& A, std::istringstream& S){ cmd_DIR(A,S);       });
    registry().add("COLOR",     [](DbArea& A, std::istringstream& S){ cmd_COLOR(A,S);     });
    registry().add("CHRISTMAS", [](DbArea& A, std::istringstream& S){ cmd_CHRISTMAS(A,S); });

    registry().add("!",    [](DbArea& A, std::istringstream& S) { cmd_BANG(A,S); });
    registry().add("BANG", [](DbArea& A, std::istringstream& S) { cmd_BANG(A,S); });
    registry().add("WEB",  [](DbArea& A, std::istringstream& S) { cmd_WEB(A,S);  });
    registry().add("IMAGE",[](DbArea& A, std::istringstream& S) { cmd_IMAGE_DISPLAY(A,S); });

    registry().add("CALC",      [](DbArea& A, std::istringstream& S){ cmd_CALC(A,S);      });
    registry().add("CALCWRITE", [](DbArea& A, std::istringstream& S){ cmd_CALCWRITE(A,S); });
    registry().add("BOOLEAN",   [](DbArea& A, std::istringstream& S){ cmd_BOOLEAN(A,S);   });
    registry().add("FORMULA",   [](DbArea& A, std::istringstream& S){ cmd_FORMULA(A,S);   });

    registry().add("EVAL",      [](DbArea& A, std::istringstream& S){ cmd_EVALUATE(A,S);  });
    registry().add("NORMALIZE", [](DbArea& A, std::istringstream& S){ cmd_NORMALIZE(A,S); });

    registry().add("SQL",      [](DbArea& A, std::istringstream& S){ cmd_SQL(A,S);        });
    registry().add("SQLSEL",   [](DbArea& A, std::istringstream& S){ cmd_SQL_SELECT(A,S); });
    registry().add("WHERE",    [](DbArea& A, std::istringstream& S){ cmd_WHERE(A,S);      });
    registry().add("SMARTLIST",[](DbArea& A, std::istringstream& S){ cmd_SMARTLIST(A,S);  });
    registry().add("INSERT",   [](DbArea& A, std::istringstream& S){ cmd_SQL_INSERT(A,S); });
    registry().add("UPDATE",   [](DbArea& A, std::istringstream& S){ cmd_SQL_UPDATE(A,S); });
    registry().add("SHOW",     [](DbArea& A, std::istringstream& S){ cmd_SQL_SHOW(A,S);   });
    registry().add("SQLERASE", [](DbArea& A, std::istringstream& S){ cmd_SQL_ERASE(A,S);  });
    registry().add("DBAREA",   [](DbArea& A, std::istringstream& S){ cmd_DBAREA(A,S);     });
    registry().add("DBAREAS",  [](DbArea& A, std::istringstream& S){ cmd_DBAREAS(A,S);    });
    registry().add("WA",       [](DbArea& A, std::istringstream& S){ cmd_WAMREPORT(A,S);  });

    registry().add("SQLITE", [](DbArea& A, std::istringstream& S){ cmd_SQLITE(A,S); });
    registry().add("SQLVER", [](DbArea& A, std::istringstream& S){ cmd_SQLVER(A,S); });

    registry().add("SCAN",    [](DbArea& A, std::istringstream& S){ cmd_SCAN(A,S);    });
    registry().add("ENDSCAN", [](DbArea& A, std::istringstream& S){ cmd_ENDSCAN(A,S); });
    registry().add("LOOP",    [](DbArea& A, std::istringstream& S){ cmd_LOOP(A,S);    });
    registry().add("ENDLOOP", [](DbArea& A, std::istringstream& S){ cmd_ENDLOOP(A,S); });

    registry().add("SCAN_BUFFER", [](DbArea& A, std::istringstream& S){ cmd_SCAN_BUFFER(A,S); });
    registry().add("LOOP_BUFFER", [](DbArea& A, std::istringstream& S){ cmd_LOOP_BUFFER(A,S); });

    registry().add("SORT",   [](DbArea& A, std::istringstream& S){ cmd_SORT(A,S); });

    registry().add("IF",     [](DbArea& A, std::istringstream& S){ cmd_IF(A,S);    });
    registry().add("ELSE",   [](DbArea& A, std::istringstream& S){ cmd_ELSE(A,S);  });
    registry().add("ENDIF",  [](DbArea& A, std::istringstream& S){ cmd_ENDIF(A,S); });

    registry().add("UNTIL",    [](DbArea& A, std::istringstream& S){ cmd_UNTIL(A,S);     });
    registry().add("ENDUNTIL", [](DbArea& A, std::istringstream& S){ cmd_ENDUNTIL(A,S);  });
    registry().add("WHILE",    [](DbArea& A, std::istringstream& S){ cmd_WHILE(A,S);     });
    registry().add("ENDWHILE", [](DbArea& A, std::istringstream& S){ cmd_ENDWHILE(A,S);  });

    registry().add("HELP",     [](DbArea& A, std::istringstream& S){ cmd_HELP(A,S);    });
    registry().add("TEST",     [](DbArea& A, std::istringstream& S){ cmd_TEST(A,S);    });
    registry().add("FOXHELP",  [](DbArea& A, std::istringstream& S){ cmd_FOXHELP(A,S); });
    registry().add("BETA",     [](DbArea& A, std::istringstream& S){ cmd_BETA(A,S);    });
    registry().add("PSHELL",   [](DbArea& A, std::istringstream& S){ cmd_PSHELL(A,S);  });


    registry().add("CMDHELP",      [](DbArea& A, std::istringstream& S){ cmd_CMDHELP(A,S); });
    registry().add("COMMANDSHELP", [](DbArea& A, std::istringstream& S){ cmd_CMDHELP(A,S); });
    registry().add("CMDHELPCHK",   [](DbArea& A, std::istringstream& S){ cmd_CMDHELPCHK(A,S); });
    registry().add("CMDREL",       [](DbArea& A, std::istringstream& S){ cmd_CMDREL(A,S); });
    registry().add("CMDARGCHK",    [](DbArea& A, std::istringstream& S){ cmd_CMDARGCHK(A,S); });

    registry().add("DOTSCRIPT",    [](DbArea& A, std::istringstream& S){ cmd_DOTSCRIPT(A,S); });
    registry().add("VAR",          [](DbArea& A, std::istringstream& S){ cmd_VAR(A,S);       });
    registry().add("ZIP",          [](DbArea& A, std::istringstream& S){ cmd_ZIP(A,S);       });

    registry().add("ECHO",         [](DbArea& A, std::istringstream& S){ cmd_ECHO(A,S);     });
    registry().add("VERSION",      [](DbArea& A, std::istringstream& S){ cmd_VERSION(A,S);  });
    registry().add("ABOUT",        [](DbArea& A, std::istringstream& S){ cmd_ABOUT(A,S);    });

    registry().add("INIT",         [](DbArea& A, std::istringstream& S){ cmd_INIT(A,S);     });
    registry().add("SHUTDOWN",     [](DbArea& A, std::istringstream& S){ cmd_SHUTDOWN(A,S); });
    registry().add("SHOWINI",      [](DbArea& A, std::istringstream& S){ cmd_SHOWINI(A,S);  });
    registry().add("TABLEMETA",    [](DbArea& A, std::istringstream& S){ cmd_TABLEMETA(A,S);});

    registry().add("FIRST",        [](DbArea& A, std::istringstream& S){ cmd_FIRST(A,S);    });
    registry().add("NEXT",         [](DbArea& A, std::istringstream& S){ cmd_NEXT(A,S);     });
    registry().add("PRIOR",        [](DbArea& A, std::istringstream& S){ cmd_PRIOR(A,S);    });
    registry().add("LAST",         [](DbArea& A, std::istringstream& S){ cmd_LAST(A,S);     });

//  Education
    registry().add("COBOL",        [](DbArea& A, std::istringstream& S){ cmd_COBOL(A,S);    });

//  MSSQL et al Import/Export
    registry().add("IMPORTSQL",    [](DbArea& A, std::istringstream& S){ cmd_IMPORTSQL(A,S); });
    registry().add("EXPORTSQL",    [](DbArea& A, std::istringstream& S){ cmd_EXPORTSQL(A,S); });
}

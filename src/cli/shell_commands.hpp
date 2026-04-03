#pragma once

#include <sstream>
#include <string>

#include "xbase.hpp"  // xbase::DbArea, xbase::XBaseEngine

// Keep the command prototypes in this header so shell_commands.cpp doesn't
// need to redeclare them (avoids drift / duplicate declarations).
using xbase::DbArea;

// -----------------------------------------------------------------------------
// Shell command registration entrypoint
extern "C" void register_shell_commands(xbase::XBaseEngine& eng, bool include_ui_cmds);

// -----------------------------------------------------------------------------
// Command prototypes (external linkage required)
// These are implemented in cmd_*.cpp files and in shell_commands.cpp
// -----------------------------------------------------------------------------
// * ---- TVision Prototypes
void cmd_TVISION(DbArea&, std::istringstream&);
void cmd_FOXPRO(DbArea&, std::istringstream&);
void cmd_FOXTALK(DbArea&, std::istringstream&);
void cmd_GENERIC(DbArea&, std::istringstream&);
void cmd_TTESTAPP(DbArea&, std::istringstream&);
void cmd_RECORDVIEW(DbArea&, std::istringstream&);
void cmd_RECORD(DbArea&, std::istringstream&);
void cmd_BROWSETV(DbArea&, std::istringstream&);
void cmd_FOX_PALETTE(DbArea&, std::istringstream&);
void cmd_CHRISTMAS(DbArea&, std::istringstream&);

void browse_bind_invoke(void (*fn)(DbArea&, const std::string&));

void cmd_USE(DbArea&, std::istringstream&);
void cmd_CLOSE(DbArea&, std::istringstream&);
void cmd_LIST(DbArea&, std::istringstream&);
void cmd_LIST_LMDB(DbArea&, std::istringstream&);
void cmd_COPY(DbArea&, std::istringstream&);
void cmd_ERASE(DbArea&, std::istringstream&);
void cmd_EXPORT(DbArea&, std::istringstream&);
void cmd_IMPORT(DbArea&, std::istringstream&);
void cmd_APPEND(DbArea&, std::istringstream&);
void cmd_APPEND_BLANK(DbArea&, std::istringstream&);
void cmd_GO(DbArea&, std::istringstream&);
void cmd_TOP(DbArea&, std::istringstream&);
void cmd_BOTTOM(DbArea&, std::istringstream&);
void cmd_GOTO(DbArea&, std::istringstream&);
void cmd_SKIP(DbArea&, std::istringstream&);
void cmd_COUNT(DbArea&, std::istringstream&);
void cmd_DISPLAY(DbArea&, std::istringstream&);
void cmd_DELETE(DbArea&, std::istringstream&);
void cmd_RECALL(DbArea&, std::istringstream&);
void cmd_TURBOPACK(DbArea&, std::istringstream&);
void cmd_PACK(DbArea&, std::istringstream&);
void cmd_COLOR(DbArea&, std::istringstream&);
void cmd_FIELDS(DbArea&, std::istringstream&);
void cmd_FIELDMGR(DbArea&, std::istringstream&);

// Reporting
void cmd_SIMPLE_BROWSER(DbArea&, std::istringstream&);
void cmd_BROWSE(DbArea&, std::istringstream&);
void cmd_BROWSER(DbArea&, std::istringstream&);
void cmd_BROWSETUI(DbArea&, std::istringstream&);
void cmd_RBROWSE(DbArea&, std::istringstream&);
void cmd_SMART_BROWSER(DbArea&, std::istringstream&);
void cmd_GPS(DbArea&, std::istringstream&);

void cmd_FIND(DbArea&, std::istringstream&);
void cmd_SEEK(DbArea&, std::istringstream&);

void cmd_SET(DbArea&, std::istringstream&);
void cmd_SET_UNIQUE(DbArea&, std::istringstream&);
void cmd_SETFILTER(DbArea&, std::istringstream&);
void cmd_SETPATH(DbArea&, std::istringstream&);
void cmd_VALIDATE(DbArea&, std::istringstream&);
void cmd_INIT(DbArea&, std::istringstream&);
void cmd_SHUTDOWN(DbArea&, std::istringstream&);
void cmd_SCAN_BUFFER(DbArea&, std::istringstream&);
void cmd_LOOP_BUFFER(DbArea&, std::istringstream&);
void cmd_AGGS(DbArea&, std::istringstream&);

// Small agg commands referenced by the registry (implemented elsewhere)
void cmd_SUM(DbArea&, std::istringstream&);
void cmd_AVG(DbArea&, std::istringstream&);
void cmd_MIN(DbArea&, std::istringstream&);
void cmd_MAX(DbArea&, std::istringstream&);
void cmd_HELP(DbArea&, std::istringstream&);

#if DOTTALK_WITH_INDEX
void cmd_INDEX(DbArea&, std::istringstream&);
void cmd_REINDEX(DbArea&, std::istringstream&);
void cmd_REBUILD(DbArea&, std::istringstream&);
void cmd_BUILDLMDB(DbArea&, std::istringstream&);
void cmd_SETINDEX(DbArea&, std::istringstream&);
void cmd_ASCEND(DbArea&, std::istringstream&);
void cmd_DESCEND(DbArea&, std::istringstream&);
void cmd_CNX(DbArea&, std::istringstream&);
void cmd_CDX(DbArea&, std::istringstream&);
void cmd_SETCNX(DbArea&, std::istringstream&);
void cmd_SETCDX(DbArea&, std::istringstream&);
void cmd_INDEXSEEK(DbArea&, std::istringstream&);
void cmd_SET_RELATIONS(DbArea&, std::istringstream&);
void cmd_RELATIONS_REFRESH(DbArea&, std::istringstream&);
void cmd_RELATIONS_LIST(DbArea&, std::istringstream&);
void cmd_TUPLE(DbArea&, std::istringstream&);
void cmd_TUPTALK(DbArea&, std::istringstream&);
void cmd_SETLMDB(DbArea&, std::istringstream&);
void cmd_SETORDER(DbArea&, std::istringstream&);
#endif

void cmd_REL(DbArea&, std::istringstream&);
void cmd_SELECT(DbArea&, std::istringstream&);
void cmd_AREA(DbArea&, std::istringstream&);
void cmd_CLEAR(DbArea&, std::istringstream&);
void cmd_CREATE(DbArea&, std::istringstream&);
void cmd_DUMP(DbArea&, std::istringstream&);
void cmd_EDIT(DbArea&, std::istringstream&);
void cmd_LOCATE(DbArea&, std::istringstream&);
void cmd_CONTINUE(DbArea&, std::istringstream&);
void cmd_LOCK(DbArea&, std::istringstream&);
void cmd_UNLOCK(DbArea&, std::istringstream&);
void cmd_RECNO(DbArea&, std::istringstream&);
void cmd_REFRESH(DbArea&, std::istringstream&);
void cmd_REPLACE(DbArea&, std::istringstream&);
void cmd_REPLACE_MULTI(DbArea&, std::istringstream&);
void cmd_SORT(DbArea&, std::istringstream&);
void cmd_STATUS(DbArea&, std::istringstream&);
void cmd_STRUCT(DbArea&, std::istringstream&);
void cmd_SCHEMA(DbArea&, std::istringstream&);
void cmd_SCHEMAS(DbArea&, std::istringstream&);
void cmd_WSREPORT(DbArea&, std::istringstream&);
void cmd_ZAP(DbArea&, std::istringstream&);
void cmd_DIR(DbArea&, std::istringstream&);
void cmd_BANG(DbArea&, std::istringstream&);
void cmd_TEST(DbArea&, std::istringstream&);
void cmd_FOXHELP(DbArea&, std::istringstream&);
void cmd_PSHELL(DbArea&, std::istringstream&);
void cmd_SCAN(DbArea&, std::istringstream&);
void cmd_ENDSCAN(DbArea&, std::istringstream&);
void cmd_LOOP(DbArea&, std::istringstream&);
void cmd_ENDLOOP(DbArea&, std::istringstream&);
void cmd_WHILE(DbArea&, std::istringstream&);
void cmd_ENDWHILE(DbArea&, std::istringstream&);
void cmd_UNTIL(DbArea&, std::istringstream&);
void cmd_ENDUNTIL(DbArea&, std::istringstream&);
void cmd_IF(DbArea&, std::istringstream&);
void cmd_ELSE(DbArea&, std::istringstream&);
void cmd_ENDIF(DbArea&, std::istringstream&);
void cmd_ECHO(DbArea&, std::istringstream&);
void cmd_VERSION(DbArea&, std::istringstream&);
void cmd_CALC(DbArea&, std::istringstream&);
void cmd_CALCWRITE(DbArea&, std::istringstream&);
void cmd_BOOLEAN(DbArea&, std::istringstream&);
void cmd_FORMULA(DbArea&, std::istringstream&);
void cmd_CMDHELP(DbArea&, std::istringstream&);
void cmd_CMDHELPCHK(DbArea&, std::istringstream&);
void cmd_CMDARGCHK(DbArea&, std::istringstream&);
void cmd_CMDREL(DbArea&, std::istringstream&);
void cmd_EVALUATE(DbArea&, std::istringstream&);
void cmd_NORMALIZE(DbArea&, std::istringstream&);
void cmd_SQL(DbArea&, std::istringstream&);
void cmd_SQL_SELECT(DbArea&, std::istringstream&);
void cmd_WHERE(DbArea&, std::istringstream&);
void cmd_SMARTLIST(DbArea&, std::istringstream&);
void cmd_DBAREA(DbArea&, std::istringstream&);
void cmd_DBAREAS(DbArea&, std::istringstream&);
void cmd_SQL_INSERT(DbArea&, std::istringstream&);
void cmd_SQL_SHOW(DbArea&, std::istringstream&);
void cmd_SQL_UPDATE(DbArea&, std::istringstream&);
void cmd_SQL_ERASE(DbArea&, std::istringstream&);
void cmd_SQLITE(DbArea&, std::istringstream&);
void cmd_SQLVER(DbArea&, std::istringstream&);
void cmd_DOTSCRIPT(DbArea&, std::istringstream&);
void cmd_VAR(DbArea&, std::istringstream&);
void cmd_TABLE(DbArea&, std::istringstream&);
void cmd_COMMIT(DbArea&, std::istringstream&);
void cmd_ROLLBACK(DbArea&, std::istringstream&);
void cmd_LMDB(DbArea&, std::istringstream&);
void cmd_LMDB_UTIL(DbArea&, std::istringstream&);
void cmd_LMDB_DUMP(DbArea&, std::istringstream&);
void cmd_WEB(DbArea&, std::istringstream&);
void cmd_IMAGE_DISPLAY(DbArea&, std::istringstream&);

// MSSQL et al
void cmd_IMPORTSQL(DbArea&, std::istringstream&);
void cmd_EXPORTSQL(DbArea&, std::istringstream&);

// Dev
void cmd_VUSE(DbArea&, std::istringstream&);
void cmd_SHOWINI(DbArea&, std::istringstream&);
void cmd_TABLEMETA(DbArea&, std::istringstream&);
void cmd_ABOUT(DbArea&, std::istringstream&);
void cmd_BETA(DbArea&, std::istringstream&);
void cmd_MEMO(DbArea&, std::istringstream&);
void cmd_WAMREPORT(DbArea&, std::istringstream&);

// FILTERS
void cmd_FIRST(DbArea&, std::istringstream&);
void cmd_NEXT(DbArea&, std::istringstream&);
void cmd_PRIOR(DbArea&, std::istringstream&);
void cmd_LAST(DbArea&, std::istringstream&);
// Education Features
void cmd_PROJECTS(DbArea&, std::istringstream&);
void cmd_ZIP(DbArea&, std::istringstream&);

// Applications
void cmd_COBOL(DbArea&, std::istringstream&);

void cmd_WHILE_BUFFER(xbase::DbArea&, std::istringstream&);
void cmd_UNTIL_BUFFER(xbase::DbArea&, std::istringstream&);





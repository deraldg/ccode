<#
  Sync HELP to reflect currently working commands.
  - Backs up src\cli\cmd_help.cpp to src\cli\cmd_help.cpp.bak-<timestamp>
  - Overwrites with a curated HELP that lists only implemented commands
Usage:
  powershell -ExecutionPolicy Bypass -File .\tools\apply_help_sync.ps1
#>

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$HelpPath = Join-Path (Get-Location) "src\cli\cmd_help.cpp"
if (-not (Test-Path -LiteralPath $HelpPath)) {
  throw "Could not find: $HelpPath"
}

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
Copy-Item -LiteralPath $HelpPath -Destination ($HelpPath + ".bak-" + $stamp)

$code = @'
#include "xbase.hpp"
#include "textio.hpp"
#include <iostream>
#include <sstream>

using namespace xbase;

static inline void out(const char* s) { std::cout << s; }

// Updated HELP: lists only implemented commands and correct usage.
void cmd_HELP(DbArea&, std::istringstream&) {
  out("CREATE <name> (<field specs>)     # make a new DBF (C/N/D/L)\n");
  out("APPEND [BLANK|-B] [n]              # append n blank records (default 1)\n");
  out("REPLACE <field> WITH <value>       # edit current record (C/N/D/L)\n");
  out("EDIT <field> WITH <value>          # alias of REPLACE\n");
  out("DUMP [TOP n] [fields...]           # vertical record dump\n");
  out("DISPLAY                            # show current record (field=value)\n");
  out("STATUS | STRUCT | RECNO | TOP | BOTTOM | GOTO <n>\n");
  out("CLEAR | CLS                        # clear the screen\n");
  out("COLOR <GREEN|AMBER|DEFAULT>\n");
  out("DIR                                 # list DBF files\n");
  out("USE <dbf> [NOINDEX]                 # open a DBF\n");
  out("LIST [n|ALL] | FIELDS | COUNT\n");
  out("INDEX ON <field> TAG <name> [ASC|DESC]\n");
  out("SEEK <field> <value> | FIND <field> <needle> | LOCATE <expr>\n");
  out("ASCEND | DESCEND | SETINDEX <tag> | SETORDER <n>\n");
  out("REFRESH                              # refresh current view/index\n");
  out("COPY | EXPORT <table> TO <file> | IMPORT <table> FROM <file> [FIELDS (...)] [LIMIT n]\n");
  out("DELETE | RECALL | PACK\n");
  out("VERSION | ZAP | BANG\n");
  out("QUIT | EXIT\n");
  out("\n");
  out("Commands:\n");
  out("  HELP              USE               QUIT\n");
  out("  EXIT              LIST              FIELDS\n");
  out("  COUNT             TOP               BOTTOM\n");
  out("  GOTO              RECNO             APPEND\n");
  out("  DELETE            DISPLAY           RECALL\n");
  out("  PACK              COPY              EXPORT\n");
  out("  IMPORT            COLOR             REPLACE\n");
  out("  EDIT              CREATE            STATUS\n");
  out("  STRUCT            INDEX             SEEK\n");
  out("  FIND              LOCATE            ASCEND\n");
  out("  DESCEND           SETINDEX          SETORDER\n");
  out("  DIR               REFRESH           VERSION\n");
  out("  ZAP               BANG\n");
}
'@

Set-Content -LiteralPath $HelpPath -Value $code -Encoding UTF8
Write-Host "cmd_help.cpp updated. Rebuild to apply." -ForegroundColor Green

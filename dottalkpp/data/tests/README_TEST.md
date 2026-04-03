# DotTalk++ TEST scripts

## Run
```
TEST /full/path/to/cnx_tag_smoke.dts /full/path/to/cnx_tag_smoke.log VERBOSE
TEST /full/path/to/reporting_snapshot.dts /full/path/to/reporting_snapshot.log VERBOSE
```
- `VERBOSE` prints each command prior to execution and logs to the given file.
- Logs include the banners/notes needed to verify TAG parsing and reporting.

## Notes
- Scripts assume `students.dbf` + `students.cnx` are in the current working directory.
- `LIST` is used without limits to keep syntax generic. Use `TOP/BOTTOM/GOTO` if needed for paging in your environment.

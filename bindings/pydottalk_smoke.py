# pydottalk_smoke.py  (ASCII only)
# Read-only smoke test for pydottalk + xbase DbArea (contract = xbase.hpp)

import os
import sys
from pathlib import Path

_MISSING = object()

def _sys_path_prepend(p: str) -> None:
    if p and p not in sys.path:
        sys.path.insert(0, p)

def _call_any(obj, names, *args, default=_MISSING):
    """
    Try a list of attribute names; if callable, call it; else return value.
    """
    for name in names:
        if hasattr(obj, name):
            v = getattr(obj, name)
            return v(*args) if callable(v) else v
    if default is not _MISSING:
        return default
    raise AttributeError(f"None of these exist on {type(obj).__name__}: {names}")

def _repo_root_from_here() -> Path:
    # expected: <repo>\bindings\pydottalk_smoke.py
    here = Path(__file__).resolve()
    return here.parent.parent

def _fielddef_parts(f):
    """
    Return (name,type,length,decimals) for a FieldDef.
    Supports:
      - attribute style: f.name / f.type / f.length / f.decimals
      - mapping style:   f['name'] / f['type'] / ...
    """
    # attribute style
    nm = getattr(f, "name", None)
    ty = getattr(f, "type", None)
    ln = getattr(f, "length", None)
    dc = getattr(f, "decimals", None)

    # mapping style (pybind sometimes exposes PODs as dict-like)
    if nm is None and hasattr(f, "get"):
        try:
            nm = f.get("name", None)
            ty = f.get("type", None)
            ln = f.get("length", None)
            dc = f.get("decimals", None)
        except Exception:
            pass

    return nm, ty, ln, dc

def _fielddef_str(f) -> str:
    nm, ty, ln, dc = _fielddef_parts(f)
    if nm is not None or ty is not None or ln is not None or dc is not None:
        return f"{nm} {ty} {ln} {dc}"

    # last resort: show public members
    pub = [a for a in dir(f) if not a.startswith("_")]
    return f"FieldDef(no exposed members; public={pub})"

def main() -> int:
    repo = _repo_root_from_here()

    print("PY:", sys.version)
    print("EXE:", sys.executable)

    # 1) ensure the built extension directory is first on sys.path
    rel_dir = os.environ.get("PYDOTTALK_BIN") or str(repo / "build" / "bindings" / "pydottalk" / "Release")
    _sys_path_prepend(rel_dir)

    # 2) import
    try:
        import pydottalk
    except Exception as e:
        print("FAIL: import pydottalk:", repr(e))
        print("  Hint: this .pyd is CP312; run with Python 3.12 (e.g., your vcpkg python).")
        print("  Also ensure Release dir is on sys.path first:")
        print("   ", rel_dir)
        return 2

    print("OK: imported pydottalk")
    print("  file   :", getattr(pydottalk, "__file__", None))
    print("  ping   :", pydottalk.ping() if hasattr(pydottalk, "ping") else "(no ping)")
    print("  version:", pydottalk.version() if hasattr(pydottalk, "version") else "(no version)")

    # 3) locate DBF directory
    dbf_dir = Path(os.environ.get("DOTTALK_DBF_DIR", str(repo / "dottalkpp" / "data" / "dbf")))
    print("  dbf_dir:", str(dbf_dir))

    # 4) list dbfs
    dbfs = []
    if hasattr(pydottalk, "list_dbf"):
        try:
            dbfs = pydottalk.list_dbf(str(dbf_dir))
        except TypeError:
            dbfs = pydottalk.list_dbf()
    print("  list_dbf count:", len(dbfs))
    for p in dbfs[:20]:
        print("   -", p)

    students = dbf_dir / "STUDENTS.dbf"
    if not students.exists():
        cand = dbf_dir / "STUDENTS.DBF"
        if cand.exists():
            students = cand

    if not students.exists():
        print("FAIL: cannot find STUDENTS.dbf/.DBF in:", str(dbf_dir))
        return 3

    # 5) open + basic navigation
    if not hasattr(pydottalk, "xbase"):
        print("FAIL: pydottalk.xbase is missing (binding not built with xbase).")
        return 4

    area = pydottalk.xbase.DbArea()
    print("  opening:", str(students))
    area.open(str(students))

    is_open = _call_any(area, ["isOpen", "is_open"], default="(unknown)")
    print("  isOpen  :", is_open)

    # Contract-first names (xbase.hpp)
    rec_count = _call_any(area, ["recCount", "rec_count", "recordCount", "record_count"])
    recno     = _call_any(area, ["recno", "recNo", "rec_no"], default=None)
    fld_count = _call_any(area, ["fieldCount", "field_count"], default=None)
    rec_len   = _call_any(area, ["recLength", "rec_length", "recordLength", "record_length", "cpr"], default=None)

    print("  recCount:", rec_count)
    if recno is not None:
        print("  recno   :", recno)
    if fld_count is not None:
        print("  fields  :", fld_count)

    # Path/name facts (contract says these should be meaningful)
    fn   = _call_any(area, ["filename"], default=None)
    ddir = _call_any(area, ["dbfDir"], default=None)
    base = _call_any(area, ["dbfBasename"], default=None)
    lname= _call_any(area, ["logicalName"], default=None)
    print("  filename    :", repr(fn)   if fn   is not None else "(missing)")
    print("  dbfDir      :", repr(ddir) if ddir is not None else "(missing)")
    print("  dbfBasename :", repr(base) if base is not None else "(missing)")
    print("  logicalName :", repr(lname)if lname is not None else "(missing)")

    if rec_len is not None:
        print("  recLength   :", rec_len)

    # top + readCurrent (some implementations require explicit read)
    _call_any(area, ["top"], default=None)
    _call_any(area, ["readCurrent", "read_current"], default=None)

    # Field metadata
    fields = _call_any(area, ["fields"], default=[])
    if fields:
        print("  FieldDef sample:")
        for f in fields[:5]:
            print("   -", _fielddef_str(f))

    # get() is 1-based per xbase.hpp
    if fld_count is None:
        fld_count = len(fields) if fields else 0

    print("  First record values (first up to 6 fields):")
    lim = min(6, int(fld_count) if fld_count else 6)
    for i in range(1, lim + 1):
        try:
            v = area.get(i)
        except Exception as e:
            v = f"(get({i}) failed: {e})"
        print(f"   [{i}] {v}")

    # bottom + close
    _call_any(area, ["bottom"], default=None)
    _call_any(area, ["readCurrent", "read_current"], default=None)
    _call_any(area, ["close"], default=None)

    print("OK: smoke test completed (read-only).")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())

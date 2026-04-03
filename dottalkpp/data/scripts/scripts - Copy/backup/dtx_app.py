# dtx_app.py  (ANSI-safe; prompt-proof batch mode)
# Python application to access database info via DotTalk++ CLI.
# This version avoids hanging by running DotTalk++ in "batch mode":
# we send commands plus QUIT, then read all output via communicate().

import os
import re
import sys
import csv
import glob
import time
import shlex
import argparse
import subprocess
from dataclasses import dataclass
from typing import List, Dict, Optional, Iterable

@dataclass
class DtxConfig:
    exe_path: str
    workdir: Optional[str] = None
    timeout_sec: float = 6.0  # overall communicate timeout

# ---------------- Batch helpers ----------------

def run_batch(cfg: DtxConfig, commands: List[str]) -> str:
    """Run DotTalk++ once, feeding commands + QUIT. Return combined stdout text."""
    if not os.path.exists(cfg.exe_path):
        raise FileNotFoundError(f"dottalkpp.exe not found: {cfg.exe_path}")

    # Always finish with QUIT so the process exits cleanly.
    script = "\n".join(commands + ["QUIT", ""])
    proc = subprocess.Popen(
        [cfg.exe_path],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=cfg.workdir or None,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    try:
        out, _ = proc.communicate(script, timeout=cfg.timeout_sec)
    except subprocess.TimeoutExpired:
        proc.kill()
        out, _ = proc.communicate()
        raise TimeoutError("DotTalk++ did not finish within the timeout.")
    return out

def find_new_csv(before: set, search_dir: str) -> Optional[str]:
    after = set(glob.glob(os.path.join(search_dir, "*.csv")))
    new_files = sorted(list(after - before), key=lambda p: os.path.getmtime(p), reverse=True)
    if new_files:
        return new_files[0]
    # Fallback: newest CSV in dir
    all_csv = sorted(glob.glob(os.path.join(search_dir, "*.csv")), key=lambda p: os.path.getmtime(p), reverse=True)
    return all_csv[0] if all_csv else None

# ---------------- Commands ----------------

def cmd_use(args, cfg: DtxConfig):
    out = run_batch(cfg, [f"USE {args.table}"])
    # Print DotTalk++ banner/result so you can see success
    sys.stdout.write(out)

def cmd_select(args, cfg: DtxConfig):
    # We rely on EXPORT -> CSV, then print rows as CSV to stdout.
    wd = cfg.workdir or os.getcwd()
    before = set(glob.glob(os.path.join(wd, "*.csv")))

    parts = []
    if args.fields:
        parts.append("FIELDS " + args.fields)
    if args.for_clause:
        parts.append("FOR " + args.for_clause)
    export_target = args.out or os.path.join(wd, "EXPORT_TEMP.csv")

    # Build batch: USE; optional ORDER later; EXPORT TO target
    batch = [f"USE {args.table}", f"EXPORT {' '.join(parts)} TO {shlex.quote(export_target)}".strip()]
    _ = run_batch(cfg, batch)  # output not needed; we will read the file

    csv_path = find_new_csv(before, wd)
    if not csv_path:
        raise FileNotFoundError("EXPORT did not produce a CSV file we can locate.")

    # Stream CSV to stdout (respect limit)
    limit = args.limit if args.limit is not None else None
    with open(csv_path, "r", newline="", encoding="utf-8", errors="replace") as f:
        reader = csv.reader(f)
        header = next(reader, None)
        if header:
            writer = csv.writer(sys.stdout, lineterminator="\n")
            writer.writerow(header)
            count = 0
            for row in reader:
                writer.writerow(row)
                count += 1
                if limit is not None and count >= limit:
                    break

def cmd_export(args, cfg: DtxConfig):
    wd = cfg.workdir or os.getcwd()
    before = set(glob.glob(os.path.join(wd, "*.csv")))

    parts = []
    if args.fields:
        parts.append("FIELDS " + args.fields)
    if args.for_clause:
        parts.append("FOR " + args.for_clause)
    export_target = args.out or os.path.join(wd, "EXPORT_TEMP.csv")

    batch = [f"USE {args.table}", f"EXPORT {' '.join(parts)} TO {shlex.quote(export_target)}".strip()]
    _ = run_batch(cfg, batch)

    csv_path = find_new_csv(before, wd)
    if not csv_path:
        raise FileNotFoundError("EXPORT did not produce a CSV file we can locate.")
    print(csv_path)

def cmd_schema(args, cfg: DtxConfig):
    # Best-effort: call FIELDS (or STATUS/STRUCT if available) and print raw output.
    out = run_batch(cfg, [f"USE {args.table}", "FIELDS"])
    # Print raw so you can see exactly what DotTalk++ returns
    sys.stdout.write(out)

# ---------------- CLI ----------------

def build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="DotTalk++ Python client (prompt-proof batch mode)")
    p.add_argument("--exe", required=True, help="Path to dottalkpp.exe")
    p.add_argument("--workdir", default=None, help="Working directory (data dir)")
    p.add_argument("--timeout", type=float, default=6.0, help="Process timeout seconds")

    sub = p.add_subparsers(dest="cmd", required=True)

    p_use = sub.add_parser("use", help="Open a table (prints DotTalk++ output)")
    p_use.add_argument("--table", required=True)
    p_use.set_defaults(func=cmd_use)

    p_sel = sub.add_parser("select", help="Select/export rows and print to stdout as CSV")
    p_sel.add_argument("--table", required=True)
    p_sel.add_argument("--fields", default=None, help="Comma-separated list of fields")
    p_sel.add_argument("--for", dest="for_clause", default=None, help='FOR clause, e.g. last_name = "Grimwood"')
    p_sel.add_argument("--limit", type=int, default=None)
    p_sel.add_argument("--out", default=None, help="Preferred export CSV path (EXPORT may ignore)")
    p_sel.set_defaults(func=cmd_select)

    p_exp = sub.add_parser("export", help="Export rows to CSV on disk; prints the path")
    p_exp.add_argument("--table", required=True)
    p_exp.add_argument("--fields", default=None, help="Comma-separated list of fields")
    p_exp.add_argument("--for", dest="for_clause", default=None, help='FOR clause')
    p_exp.add_argument("--out", default=None, help="Target CSV path (EXPORT may ignore)")
    p_exp.set_defaults(func=cmd_export)

    p_sch = sub.add_parser("schema", help="Print table field info (raw DotTalk++ output)")
    p_sch.add_argument("--table", required=True)
    p_sch.set_defaults(func=cmd_schema)

    return p

def main(argv: List[str]) -> int:
    ap = build_arg_parser()
    args = ap.parse_args(argv)
    cfg = DtxConfig(exe_path=args.exe, workdir=args.workdir, timeout_sec=args.timeout)
    args.func(args, cfg)
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

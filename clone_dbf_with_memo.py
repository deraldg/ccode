#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
clone_dbf_with_memo.py
Clones .DBF files, adds a Memo field, and populates all fields (+ memo) with plausible data.
Requires: pip install dbf
"""

import argparse, os, re, random, datetime, json, sys
from pathlib import Path

try:
    import dbf
except Exception as e:
    print("ERROR: This script requires the 'dbf' package. Install with:", file=sys.stderr)
    print("  py -3 -m pip install dbf", file=sys.stderr)
    print("or:", file=sys.stderr)
    print("  python -m pip install dbf", file=sys.stderr)
    sys.exit(1)

# ------------- Utilities -------------

def load_rules(rules_path: Path):
    if rules_path and rules_path.exists():
        with open(rules_path, "r", encoding="utf-8") as f:
            return json.load(f)
    return {}

def ci_contains(name: str, fragment: str) -> bool:
    return fragment.lower() in name.lower()

FIRST = ["Alex","Jordan","Taylor","Casey","Morgan","Avery","Riley","Quinn","Drew","Skyler","Jamie","Kendall","Reese","Harper","Rowan","Cameron","Dakota","Emerson","Logan","Peyton"]
LAST  = ["Grimwood","Smith","Johnson","Williams","Brown","Jones","Garcia","Miller","Davis","Rodriguez","Martinez","Hernandez","Lopez","Gonzalez","Wilson","Anderson","Thomas","Taylor","Moore","Jackson"]
CITIES= ["Eugene","Portland","Salem","Medford","Bend","Corvallis","Beaverton","Gresham","Springfield","Hillsboro","Newcastle","Albany","Grants Pass","Ashland","Klamath Falls"]
STATES= ["OR","WA","CA","ID","NV","AZ","UT","WY","MT","CO"]
WORDS = ["system","project","module","integration","performance","analysis","testing","design","prototype","deployment","support","training","documentation","workflow","refactor","indexing","memo","storage","reliability","upgrade","iteration","compatibility"]

def fake_first(): return random.choice(FIRST)
def fake_last():  return random.choice(LAST)
def fake_full():  return f"{fake_first()} {fake_last()}"
def fake_city():  return random.choice(CITIES)
def fake_state(): return random.choice(STATES)
def fake_zip():   return f"{random.randint(97001, 97999)}"
def fake_phone(): return f"({random.randint(200,989)}) {random.randint(200,999)}-{random.randint(1000,9999)}"
def fake_email(first,last): return f"{first.lower()}.{last.lower()}@example.com"

def rand_date_within_years(years=10):
    today = datetime.date.today()
    delta_days = random.randint(0, years*365)
    return today - datetime.timedelta(days=delta_days)

def rand_birth():
    # age 18-80
    age = random.randint(18,80)
    return datetime.date.today() - datetime.timedelta(days=age*365 + random.randint(0,364))

def rand_money(minv=30000, maxv=120000, decimals=2):
    return round(random.uniform(minv, maxv), decimals)

def rand_float(minv=0.0, maxv=1.0, decimals=2):
    return round(random.uniform(minv, maxv), decimals)

def rand_int(minv=0, maxv=100):
    return random.randint(minv, maxv)

def rand_bool():
    return random.choice([True, False])

def make_memo_paragraph():
    words = random.choices(WORDS, k=random.randint(12, 22))
    s = " ".join(words).capitalize() + "."
    return s

def make_memo_text(min_paras=1, max_paras=3):
    return "\n\n".join(make_memo_paragraph() for _ in range(random.randint(min_paras, max_paras)))

def pick_generator(name, rules):
    # Check rules by fragment
    for key, spec in rules.items():
        if key.lower() in name.lower():
            return spec
    return None

def coerce_to_width(text, width):
    if text is None: return ""
    s = str(text)
    if len(s) > width:
        return s[:width]
    return s

# ------------- Core -------------

def clone_table_with_memo(src_path: Path, out_dir: Path, rules: dict, verbose=False):
    src = dbf.Table(str(src_path))
    src.open(dbf.READ_ONLY)
    try:
        # Determine target memo field name
        existing_names = [f.name.upper() for f in src.structure()]
        memo_name = "NOTES"
        if memo_name.upper() in existing_names:
            memo_name = "NOTES2"

        # Build new structure string with memo field
        # Example: "NAME C(30); AGE N(3,0); JOINED D; ACTIVE L; NOTES M"
        fieldspecs = []
        for f in src.structure():
            fieldspecs.append(f"{f.name} {f.type}({f.length},{f.decimal_count})" if f.type in "NF" else
                              (f"{f.name} {f.type}({f.length})" if f.type in "CM" else
                               f"{f.name} {f.type}"))
        fieldspecs.append(f"{memo_name} M")
        struct_str = "; ".join(fieldspecs)

        out_name = src_path.with_stem(src_path.stem + "_with_memo").name
        out_path = out_dir / out_name

        if verbose:
            print(f"[+] Creating: {out_path} (structure: {struct_str})")

        dst = dbf.Table(str(out_path), struct_str)
        dst.open(dbf.READ_WRITE)

        # Populate
        for rec in src:
            data = {}
            # First pass: generate core first/last for cross-field consistency
            first_guess, last_guess = fake_first(), fake_last()

            for f in dst.structure():
                if f.name == memo_name:
                    continue  # handle memo at end

                gen = pick_generator(f.name, rules)

                if f.type == "C":
                    width = f.length
                    val = None
                    if gen:
                        gt = gen.get("type","")
                        if gt == "name_first":
                            val = coerce_to_width(first_guess, width)
                        elif gt == "name_last":
                            val = coerce_to_width(last_guess, width)
                        elif gt == "name_full":
                            val = coerce_to_width(f"{first_guess} {last_guess}", width)
                        elif gt == "email":
                            val = coerce_to_width(fake_email(first_guess, last_guess), width)
                        elif gt == "city":
                            val = coerce_to_width(fake_city(), width)
                        elif gt == "us_state_abbr":
                            val = coerce_to_width(fake_state(), width)
                        elif gt == "us_zip":
                            val = coerce_to_width(fake_zip(), width)
                        elif gt == "us_phone":
                            val = coerce_to_width(fake_phone(), width)
                        else:
                            val = coerce_to_width(str(rec.get(f.name, "")), width)
                    else:
                        # Fallback: echo original or generic text
                        base = str(rec.get(f.name, "")) or fake_full()
                        val = coerce_to_width(base, width)
                    data[f.name] = val

                elif f.type in ("N", "F"):
                    # numeric/float
                    if gen:
                        gt = gen.get("type","")
                        if gt == "money":
                            val = rand_money(gen.get("min", 30000), gen.get("max", 120000))
                        elif gt == "float":
                            val = rand_float(gen.get("min", 0.0), gen.get("max", 1.0), gen.get("decimals", 2))
                        elif gt == "int":
                            val = rand_int(gen.get("min", 0), gen.get("max", 100))
                        else:
                            val = rand_int(0, 9999)
                    else:
                        # Try to keep original numeric if present, else random
                        try:
                            v = rec.get(f.name, None)
                            if v is None or (isinstance(v, str) and not v.strip()):
                                val = rand_int(0, 9999)
                            else:
                                val = v
                        except Exception:
                            val = rand_int(0, 9999)
                    data[f.name] = val

                elif f.type == "D":
                    if gen:
                        gt = gen.get("type","")
                        if gt == "date_recent":
                            val = rand_date_within_years(10)
                        elif gt == "date_birthlike":
                            val = rand_birth()
                        else:
                            val = rand_date_within_years(10)
                    else:
                        val = rand_date_within_years(10)
                    data[f.name] = val

                elif f.type == "L":
                    data[f.name] = rand_bool()

                else:
                    # Other types, copy if possible
                    data[f.name] = rec.get(f.name, None)

            # Create memo text
            memo_text = make_memo_text()
            data[memo_name] = memo_text

            dst.append(data)

        dst.close()
        if verbose:
            print(f"[✓] Wrote: {out_path} (+ memo file)")
        return out_path
    finally:
        src.close()

def main():
    ap = argparse.ArgumentParser(description="Clone DBFs, add Memo field, populate with plausible data")
    ap.add_argument("--source", required=True, help="Source folder containing .dbf files")
    ap.add_argument("--out", required=True, help="Output folder for cloned DBFs")
    ap.add_argument("--rules", default="schema_rules.json", help="Optional schema rules JSON")
    ap.add_argument("--verbose", action="store_true", help="Verbose logging")
    args = ap.parse_args()

    src_dir = Path(args.source).expanduser()
    out_dir = Path(args.out).expanduser()
    out_dir.mkdir(parents=True, exist_ok=True)

    rules = load_rules(Path(args.rules))

    dbfs = list(src_dir.glob("*.dbf"))
    if not dbfs:
        print(f"No .dbf files found in: {src_dir}", file=sys.stderr)
        sys.exit(2)

    for path in dbfs:
        try:
            clone_table_with_memo(path, out_dir, rules, verbose=args.verbose)
        except Exception as e:
            print(f"[!] Failed on {path.name}: {e}", file=sys.stderr)

if __name__ == "__main__":
    main()

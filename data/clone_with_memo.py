# clone_with_memo.py
# Clone every .dbf in current folder -> *_memo.dbf with a NOTES memo field.
# Requires:  py -3 -m pip install dbf
import os, random, datetime, sys
from pathlib import Path
import dbf

WORDS = ["system","project","module","integration","analysis","testing","design",
         "support","training","workflow","indexing","memo","storage","upgrade","iteration"]
NAMES = ["Alice","Bob","Charlie","Diana","Eve","Frank","Grace","Heidi","Ivan","Judy","Mallory","Peggy"]
CITIES= ["Eugene","Portland","Medford","Bend","Corvallis","Ashland","Hillsboro"]

def fake_memo():
    paras = []
    for _ in range(random.randint(1,3)):
        w = " ".join(random.choices(WORDS, k=random.randint(10,18))).capitalize() + "."
        paras.append(w)
    return "\n\n".join(paras)

def fake_value(ftype, flen, fdec, fname):
    base = (fname or "").lower()
    if ftype == "C":
        if "name" in base:
            v = random.choice(NAMES)
        elif "city" in base:
            v = random.choice(CITIES)
        elif "email" in base:
            v = (random.choice(NAMES).lower() + "." + random.choice(NAMES).lower() + "@example.com")
        else:
            v = "Val" + str(random.randint(100,999))
        return (v[:flen]).ljust(flen)
    elif ftype in ("N","F"):
        if (fdec or 0) > 0:
            return round(random.uniform(0,100), fdec)
        else:
            return random.randint(0, 100)
    elif ftype == "D":
        return datetime.date.today() - datetime.timedelta(days=random.randint(0, 3650))
    elif ftype == "L":
        return random.choice([True, False])
    else:
        return None

def parse_fields(struct_str):
    """Parse dbf.Table.structure() string into list of (name, type, length, decimals)."""
    fields = []
    parts = [p.strip() for p in struct_str.split(";") if p.strip()]
    for p in parts:
        toks = p.split()
        if not toks:
            continue
        name = toks[0]
        rest = "".join(toks[1:]) if len(toks) > 1 else ""
        ftype = rest[:1] if rest else "C"
        flen, fdec = 0, 0
        if "(" in rest and ")" in rest:
            inside = rest[rest.find("(")+1:rest.find(")")]
            if "," in inside:
                a,b = inside.split(",",1)
                flen = int(a.strip()); fdec = int(b.strip())
            else:
                flen = int(inside.strip())
        fields.append((name, ftype, flen, fdec))
    return fields

def clone_table(src_path: Path):
    src = dbf.Table(str(src_path))
    src.open(dbf.READ_ONLY)

    struct_str = src.structure()
    fields = parse_fields(struct_str)
    existing_names = {n.upper() for (n,_,_,_) in fields}
    memo_name = "NOTES" if "NOTES" not in existing_names else "NOTES2"

    out_path = src_path.with_name(src_path.stem + "_memo.dbf")
    if out_path.exists():
        out_path.unlink()

    dst_struct = struct_str.rstrip().rstrip(";") + f"; {memo_name} M"
    dst = dbf.Table(str(out_path), dst_struct)
    dst.open(dbf.READ_WRITE)

    meta = {n:(t,l,d) for (n,t,l,d) in fields}

    for rec in src:
        row = {}
        for n,(t,l,d) in meta.items():
            try:
                val = rec[n]
            except Exception:
                val = None
            if val is None or (isinstance(val, str) and val.strip() == ""):
                val = fake_value(t, l, d, n)
            if t == "C" and isinstance(val, str) and l > 0:
                val = val[:l]
            row[n] = val
        row[memo_name] = fake_memo()
        dst.append(row)

    dst.close(); src.close()
    print(f"Created: {out_path}")

def main():
    here = Path(".")
    dbfs = [p for p in here.iterdir() if p.suffix.lower()==".dbf" and not p.stem.lower().endswith("_memo")]
    if not dbfs:
        print("No .dbf files found here", file=sys.stderr)
        return
    for p in dbfs:
        try:
            clone_table(p)
        except Exception as e:
            print(f"[!] Failed on {p.name}: {e}", file=sys.stderr)

if __name__ == "__main__":
    main()

# DBF Memo Cloner

This package clones your **existing .DBF** test files into **new versions that include a Memo (M) field** named `NOTES`, and populates **all fields** with plausible data (respecting types).

It uses the Python [`dbf`](https://pypi.org/project/dbf/) library **on your machine**.

## What you get
- `clone_dbf_with_memo.py` — main script that clones/augments DBFs and fills records.
- `run_clone.ps1` — one‑liner PowerShell wrapper you can edit and run.
- `schema_rules.json` — optional mapping for smarter, per-field fake data.
- `README.md` — this file.

## Requirements (on your PC)
1. Python 3.9+
2. Install the `dbf` package:
   ```powershell
   py -3 -m pip install dbf
   ```
   or
   ```powershell
   python -m pip install dbf
   ```

## Usage
1. Put your source `.dbf` files in a folder — e.g., `C:\Users\deral\code\ccode\data`.
2. Edit `run_clone.ps1` to point to your **source** and **output** folders.
3. Run from PowerShell:
   ```powershell
   .\run_clone.ps1
   ```
   The script will create, for each `X.dbf`, a new file `X_with_memo.dbf` (and its memo file) in the output folder.

## Notes
- The memo field is named **NOTES** (type **M**). If your table already has a memo named `NOTES`, the script will use `NOTES2` to avoid collisions.
- It preserves original field names and types and tries to generate sensible values:
  - Character fields: realistic names, cities, emails, etc., based on the field name.
  - Numeric/Float: ranged numbers with appropriate decimals.
  - Date: plausible dates in the past 10 years.
  - Logical: TRUE/FALSE balanced.
  - Memo: 1–3 short paragraphs of plausible text.
- If you have a **known schema** (like `STUDENTS`, `TEACHERS`), add rules in `schema_rules.json` to improve realism.

## Troubleshooting
- If you see `ModuleNotFoundError: No module named 'dbf'`, install the package as shown above.
- If a file is read‑only or locked, close other apps and try again.
- Works with dBase III/IV/FoxPro dialects supported by the `dbf` library.

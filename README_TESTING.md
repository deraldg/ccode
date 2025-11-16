# DotTalk++ Test Scripts & Runner — Setup & Usage

This doc explains **where each file lives**, **how to run the tests**, and **how baselines/diffs work**. Examples assume Windows + PowerShell.

---

## Repository layout (expected)

```
<repo-root>\
│  run_tests.ps1                      # UPDATED test runner (auto-finds EXE, seeds baseline, diffs)
│
├─scripts\                            # .dts scripts the TEST command consumes
│    struct_inx_active.dts            # existing
│    struct_cnx_tags.dts              # existing
│    list_order_flip.dts              # existing
│    full_baseline.dts                # existing
│    inxi_attach_switching.dts        # NEW
│    cnx_tag_keyword_form.dts         # NEW
│    banner_consistency.dts           # NEW
│
├─logs\                                # auto-created by runner; per-test logs + optional .diff files
│
├─baseline\                            # created/seeded by runner on first pass (golden logs)
│
└─build\
   └─src\
      └─Release\
         dottalkpp.exe                 # built binary the runner invokes (or provide -ExePath)
```

> If your `dottalkpp.exe` ends up somewhere else, point the runner there using `-ExePath`.

---

## The runner (`run_tests.ps1`)

### What it does
- **Finds the EXE** automatically from these paths (in order) if `-ExePath` is not provided:
  - `.\build\src\Release\dottalkpp.exe`
  - `.\build\src\Debug\dottalkpp.exe`
  - `.\build\Release\dottalkpp.exe`
  - `.\build\Debug\dottalkpp.exe`
- **Creates** `.\logs` and `.\baseline` if they don’t exist.
- **Runs** each listed `.dts` with the program’s `TEST` command, writing `.\logs\<test>.log`.
- **Seeds a baseline** (`.\baseline\<test>.log`) the first time a given test is run.
- On subsequent runs, **diffs current log vs baseline** using `fc /n` and writes `.\logs\<test>.log.diff` when differences are found.
- **Non‑zero exit** if any test fails or any diff is detected.

### Running it (from repo root)

#### 1) With explicit EXE path (recommended first run)
```powershell
powershell -ExecutionPolicy Bypass -File .\run_tests.ps1 -ExePath .\build\src\Release\dottalkpp.exe
```

#### 2) Let it auto‑find the EXE
```powershell
powershell -ExecutionPolicy Bypass -File .\run_tests.ps1
```

#### 3) From the `scripts` folder (if you prefer calling it there)
```powershell
cd .\scripts
powershell -ExecutionPolicy Bypass -File ..\run_tests.ps1 -ExePath ..\build\src\Release\dottalkpp.exe
```

#### 4) Absolute EXE path
```powershell
powershell -ExecutionPolicy Bypass -File .\run_tests.ps1 -ExePath "C:\Users\<you>\code\ccode\build\src\Release\dottalkpp.exe"
```

### Interpreting output
- On first run for a test, you’ll see **“No baseline … seeding baseline.”**
- On later runs:
  - **No differences:** `OK: <test> matches baseline.`
  - **Differences:** warning + `.diff` file saved next to the log, runner exits with code `1`.

### Typical flow
```powershell
# First pass (seed baseline for all tests)
powershell -ExecutionPolicy Bypass -File .\run_tests.ps1 -ExePath .\build\src\Release\dottalkpp.exe

# Inspect baseline if you want
Get-ChildItem .\baseline

# Make a code change ... then re-run
powershell -ExecutionPolicy Bypass -File .\run_tests.ps1

# If diffs were reported, open them
notepad .\logs\struct_cnx_tags.log.diff
```

> **Tip:** If the program’s output contains timestamps or non-deterministic text, expect benign diffs. We can filter that in the runner later if needed.

---

## The DTS scripts

All scripts live under `.\scripts\`. They are **read‑only** (no destructive commands).

| File | Purpose |
|---|---|
| `struct_inx_active.dts` | Baseline check: single‑tag INX shows as active; `STRUCT` reflects it. |
| `struct_cnx_tags.dts` | CNX: attach, set tag, check `STATUS`, `STRUCT INDEX`, and `LIST` consistency. |
| `list_order_flip.dts` | CNX: verify `ASCEND`/`DESCEND` stability across tags and list output. |
| `full_baseline.dts` | Combined INX→CNX flow for a quick smoke covering common scenarios. |
| `inxi_attach_switching.dts` | **NEW** INX attach/switch exercise ensuring active tag reporting doesn’t regress. |
| `cnx_tag_keyword_form.dts` | **NEW** Validates the `SETORDER <cnx> TAG <name> [--asc|--desc]` keyword form. |
| `banner_consistency.dts` | **NEW** Ensures the LIST banner (`CNX ORDER: file … TAG … ASC/DESC`) matches state. |

### Running a single script manually (without the runner)
From repo root:
```powershell
# Make sure logs directory exists if you want to capture output
mkdir .\logs -ErrorAction SilentlyContinue

.\build\src\Release\dottalkpp.exe "TEST scripts\struct_cnx_tags.dts logs\struct_cnx_tags.log VERBOSE"
type .\logs\struct_cnx_tags.log
```

From inside `scripts\`:
```powershell
..\build\src\Release\dottalkpp.exe "TEST struct_cnx_tags.dts ..\logs\struct_cnx_tags.log VERBOSE"
```

> The `TEST` command syntax is: `TEST <scriptfile> [<logfile>] [VERBOSE]`

---

## Baselines & diffs

- Baselines live in `.\baseline\*.log`.
- Current logs live in `.\logs\*.log`.
- Diffs (when present) live as `.\logs\*.log.diff` and use `fc /n` formatting (line‑numbered).

**Re‑seeding a baseline for one test** (after an intentional output change):
```powershell
Copy-Item .\logs\struct_cnx_tags.log .\baseline\struct_cnx_tags.log -Force
```

**Wipe all logs if needed:**
```powershell
Remove-Item .\logs\* -Force
```

**Reset all baselines (start fresh):**
```powershell
Remove-Item .\baseline\* -Force
powershell -ExecutionPolicy Bypass -File .\run_tests.ps1 -ExePath .\build\src\Release\dottalkpp.exe
```

---

## Common pitfalls & fixes

- **Running the wrong runner:** If you still see a hard‑coded `..\\build\\Release\\dottalkpp.exe` path in error messages, you’re using an **old** script. Ensure `run_tests.ps1` at repo root contains the text `Tried:` (a hallmark of the new version).  
  ```powershell
  Select-String -Path .\run_tests.ps1 -Pattern 'Tried:'
  ```

- **EXE not found:** Provide `-ExePath` explicitly or build the binary. Verify path:  
  ```powershell
  Get-Item .\build\src\Release\dottalkpp.exe
  ```

- **Relative paths from `scripts\`:** If you `cd scripts`, remember EXE is typically `..\\build\\src\\Release\\dottalkpp.exe`.

- **Missing `logs\` or `baseline\`:** The new runner creates them automatically. If you bypass the runner, create them yourself.

---

## CI hint (optional)

Example GitHub Actions step to run the suite after building the EXE:

```yaml
- name: Run DotTalk++ DTS tests
  shell: pwsh
  run: |
    Set-ExecutionPolicy Bypass -Scope Process -Force
    ./run_tests.ps1 -ExePath ./build/src/Release/dottalkpp.exe
```

---

## Troubleshooting commands (quick copy/paste)

```powershell
# Which run_tests.ps1 am I executing?
Get-Command .\run_tests.ps1 | Format-List Source

# Confirm the EXE exists
Get-Item .\build\src\Release\dottalkpp.exe

# Run one test directly
mkdir .\logs -ErrorAction SilentlyContinue
.\build\src\Release\dottalkpp.exe "TEST scripts\struct_inx_active.dts logs\struct_inx_active.log VERBOSE"
```

---

## Contact

If you want a runner switch for **cleaning logs**, or a **-Tests** filter to run a subset, say the word and I’ll drop a minimal patch.

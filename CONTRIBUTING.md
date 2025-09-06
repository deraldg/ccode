# Contributing

Thanks for helping improve **ccode**. This repo follows Derald's PDLC and INI defaults.

## Ground Rules
- **GitFlow** branches: `main`, `develop`, `feature/*`, `release/*`, `hotfix/*`.
- **Conventional Commits**: `type(scope)!: summary`.
- Keep PRs focused and small. Update **CHANGELOG.md** under **[Unreleased]**.

## Workflow
1. Create an issue (bug/feature).
2. Branch from `develop`: `feature/<short-slug>` or `fix/<short-slug>`.
3. Implement:
   - C++: CMake, MSVC 2022 (v143). Enable `/W4`; consider `/analyze` locally.
   - Python: `black`, `flake8`, `mypy` (min 3.10). Prefer stdlib & dataclasses.
   - Java: JDK 21, Gradle, `google-java-format`.
   - Node: npm, ESM, prettier/eslint.
4. Tests: add/update unit tests where applicable.
5. Update docs & **CHANGELOG.md**.
6. Open PR to `develop` using the PR template. Ensure CI is green.
7. Maintainer merges to `develop`. Releases cut from `main` via tags.

## Commit Message Format
```
<type>(<scope>)!: <short summary>
<blank line>
<longer description, optional>
```
**Types:** feat, fix, perf, refactor, docs, build, ci, chore, test, style

## Versioning & Release
- **SemVer**; tags like `v0.5.0` trigger the **Release** workflow.
- Keep a running **[Unreleased]** section; move entries into a versioned block on tag.

## Local Build Quickstarts
**C++ (Windows/MSVC)**
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -- /m
```

**Python (pycrud)**
```powershell
cd pycrud
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt  # or pip install pytest sqlalchemy fastapi uvicorn pydantic
pytest -q
```

**Java (Web UI)**
```powershell
cd dottalk-webui
.\mvnw -v  # or gradlew if Gradle
```

## Style Notes
- **Line endings:** CRLF (auto-normalized); `.sh/.yml` use LF.
- **Encoding:** UTF-8; no BOM.
- Avoid secrets/PII in code and issues.

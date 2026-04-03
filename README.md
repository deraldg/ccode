# x64base / DotTalk++

DotTalk++ is a modern C++ xBase-style database engine and command shell.

It preserves the workflow of classic dBASE/FoxPro systems while introducing a
modern 64-bit architecture, extensible indexing, scripting, and integration paths.

---

## Overview

DotTalk++ is the core engine of the **x64base** project.

The goal is to build a practical, understandable, and extensible system that:

- preserves proven xBase command workflows
- supports modern storage and indexing approaches
- enables scripting, automation, and data conversion
- remains suitable for both real work and educational use

This is not a nostalgic clone. It is a forward-looking rebuild grounded in what worked.

---

## Current Status

**Active development — approaching cohesive beta**

Current focus:

- engine stability and correctness
- LMDB-backed CDX-style indexing
- command surface consistency (LIST, SMARTLIST, SEEK, ORDER)
- scripting (DotScript) expansion
- Python integration (pydottalk)
- repo and packaging cleanup

Expect ongoing changes.

---

## Key Features

### Engine
- modern C++ (64-bit)
- DBF-oriented table model
- work areas and cursor-based navigation
- CLI-first architecture

### Command Surface
- familiar commands:
  - `USE`, `SELECT`, `LIST`, `DISPLAY`
  - `SEEK`, `LOCATE`, `COUNT`
  - `INDEX`, `SET ORDER`
- evolving SMARTLIST / expression-driven listing
- help and discovery tools

### Indexing
- multi-system indexing approach
- CDX-style container model
- LMDB-backed index storage
- open architecture for future index engines

### Scripting
- DotScript automation layer
- loops, conditions, variables (expanding)
- regression and shakedown support

### Integration
- Python bindings (`pydottalk`)
- groundwork for SQL and external data systems
- planned ETL and conversion workflows

---

## Architecture Principles

- CLI is canonical
- engine defines truth (not UI layers)
- keep architecture open for evolution
- avoid premature locking into one backend
- prioritize correctness over performance (for now)
- preserve clarity over complexity

---

## Repository Layout

```text
bindings/       Python integration
docs/           documentation and notes
dottalkpp/      runtime structure and project assets
include/        headers
scripts/        build and helper scripts
src/            core engine and CLI
tests/          validation and regression
third_party/    vendored libraries
tools/          maintenance utilities
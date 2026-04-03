# DotTalk++

DotTalk++ is an open-source interactive database runtime and scripting environment written in C++.

It is inspired by the historical **xBase family of database systems** and designed for exploration,
experimentation, and education around indexed data systems.

DotTalk++ began as a weekend database experiment in 1993 and grew into a modern
64-bit xBase-inspired runtime more than thirty years later.

Website: https://derald.com

---

## Components

The project is organized as several cooperating components.

### dottalk++

The interactive command shell used to explore and manipulate databases.

Features include:

- interactive command interpreter
- DBF table navigation
- index-driven record traversal
- relational traversal (REL ENUM)
- tuple projection
- scripting via DotScript

---

### xbase

Core runtime library providing:

- DBF table management
- record navigation
- field metadata
- work area management
- record buffering
- session state

This layer implements the behavior traditionally associated with xBase database environments.

---

### xindex

Indexing subsystem used by DotTalk++.

Features include:

- ordered index navigation
- high-performance key lookup
- 64-bit addressing
- LMDB-based storage

---

### PyDotTalk

PyDotTalk (`pydottalk.exe`) provides a bridge between DotTalk++ and Python-oriented
automation workflows.

It allows experimentation with the DotTalk runtime from Python environments.

---

## Features

DotTalk++ includes:

- interactive command shell
- DBF table navigation
- index-driven ordering
- relational traversal
- tuple projection
- structured help catalogs
- scripting support
- record buffering
- 64-bit DBF extensions
- LMDB-based indexing

---

## Educational Use

DotTalk++ is designed as a **learning environment for database concepts**.

The runtime exposes internal database state directly, including:

- current work area
- record pointer
- active index order
- filters
- relations between tables

This makes it possible to observe database behavior step-by-step while issuing commands.

The system includes help catalogs describing:

- records and fields
- indexing
- filtering
- relational traversal
- database session state

DotTalk++ can therefore be used to teach:

- database fundamentals
- index-driven data systems
- relational navigation
- query behavior

---

## Origins

The conceptual origins of DotTalk++ trace back to **1993**, when the author created
a small experimental database system written in ANSI C.

The original program was named **xbase** and implemented:

- fixed-length record storage
- a simple in-memory B-tree index
- interactive record navigation

The project served as a personal exploration of database engine design.

In **2025**, the earlier work was revisited and used as the conceptual basis
for a modern rewrite in C++. The result became **DotTalk++**, which expands
the original ideas into a modular runtime system designed for experimentation,
education, and exploration of database concepts.

---

## Heritage

DotTalk++ draws inspiration from the classic **xBase lineage**:

| System | Contribution |
|------|-------------|
| dBase | early interactive DBF database shell |
| Clipper | compiled xBase applications |
| FoxPro | high-performance indexed DBF navigation |

DotTalk++ preserves the **interactive shell model** while extending it with modern runtime capabilities.

---

## License

DotTalk++ is released under the **MIT License**.

MIT License

Copyright (c) 2025 Derald Grimwood

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files to deal in the Software
without restriction...

---

## Author

Derald Grimwood

Dedicated to: Kathy Grimwood
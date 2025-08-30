# DotTalk++ Boolean Expressions — Drop‑in Package (Milestone 1)

This bundle gives you:
1) A self-contained **expression engine** (`include/dottalk/expr/*`, `src/cli/expr/*`).
2) A tiny **FOR clause extractor** for handlers (`for_parser.hpp/.cpp`).
3) A minimal **xbase glue** header to map your `DbArea` to the evaluator (`glue_xbase.hpp`).
4) Example **COUNT** and **LOCATE** handlers showing the integration points.

> Milestone 1 supports: identifiers, numbers, strings, comparisons (= == != < <= > >=),
> boolean ops (NOT, AND, OR and dotted `.NOT. .AND. .OR.`), and parentheses.

## Quick integration

1. **Add includes & sources** to your build:
   - Include directory: `include/`
   - Sources (drop in under your tree): `src/cli/expr/*.cpp`

   CMake snippet (append to your main `CMakeLists.txt`):
   ```cmake
   # ---- DotTalk++ Expr (M1) ------------------------------------------------
   target_include_directories(dottalkpp PRIVATE ${CMAKE_SOURCE_DIR}/include)

   set(DOTTALK_EXPR_SOURCES
     src/cli/expr/lexer.cpp
     src/cli/expr/parser.cpp
     src/cli/expr/eval.cpp
     src/cli/expr/api.cpp
     src/cli/expr/for_parser.cpp
   )
   target_sources(dottalkpp PRIVATE ${DOTTALK_EXPR_SOURCES})
   ```

2. **Map your DbArea field access** in `include/dottalk/expr/glue_xbase.hpp`:
   - By default, it expects you to define two macros **before** including the header (e.g. in a central header like `xbase.hpp` or `cmd_common.hpp`):
     ```cpp
     #define DOTTALK_GET_FIELD_STR(area, name)  (area.getFieldAsString(name))
     #define DOTTALK_GET_FIELD_NUM(area, name)  (area.getFieldAsNumber(name))
     #include "dottalk/expr/glue_xbase.hpp"
     ```
   - If your API differs, change those macros to match your getters.
   - If you have locale/case rules, you can centralize them later.

3. **Patch your command handlers**:
   - See `patches/cmd_count_add_for_example.cpp`
   - See `patches/cmd_locate_add_for_example.cpp`

   These are example *full* files. Either merge the relevant parts into your existing handlers,
   or replace your handlers and adapt the iteration bits to your `DbArea` API.

## Notes

- The engine is generic; only the tiny glue (`glue_xbase.hpp`) knows about `DbArea`.
- M3 will add `$`/`!$` (contains / not-contains), case controls, etc.
- For index fast-paths (like `FIELD = "LIT"` on active tag), add that optimization later—after correctness.


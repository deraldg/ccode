// README_integration.txt
DotTalk++ — CLI-side Filter Integration (Layering-safe)

1) Leave xbase and xindex projects untouched. Revert any edits to include/xbase.hpp
   and delete src/xbase/dbarea_filter.cpp (if added). The filter system now lives
   fully in the CLI target to avoid cross-project include problems.

2) Add these files to the CLI target (dottalkpp.vcxproj / CMake target):
   - src/cli/filters/filter_registry.hpp
   - src/cli/filters/filter_registry.cpp
   - src/cli/cmd_setfilter.cpp   (drop-in replacement)

3) Update each record-iterating command to call the visibility helper:
   Example (LIST):
     // existing setup...
     PredicateContext ctx; ctx.bind(&rv);
     // Deleted visibility (if you have SET DELETED ON logic):
     if (area.isDeleted() && set_deleted_on) continue;
     // Filter + FOR
     if (!filter::visible(&area, ctx, forAst.get())) continue;

   Do the same for COUNT, SCAN, LOCATE, FIND, SEEK (post-position), DISPLAY (if needed).

4) CMake:
   Ensure the CLI target has include paths for the expression headers used here:
     predicate_parser.hpp, predicate_eval.hpp
   These already live with the CLI, so it should compile without touching xbase/xindex.

5) Behavior:
   - SET FILTER persists per-DbArea in the registry (keyed by DbArea*).
   - Clearing filter removes state from the map.
   - Filters apply before FOR; WHILE is a sequential stop rule in the command loop.

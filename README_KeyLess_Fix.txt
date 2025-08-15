KeyLess compile fix (MSVC incomplete type error)

Symptom:
  error C2139: 'xindex::KeyLess': an undefined class is not allowed as an argument to compiler intrinsic type trait '__is_empty'
  ... from include/xindex/bptree_backend.hpp when instantiating std::multimap<..., xindex::KeyLess, ...>

Root cause:
  std::multimap stores the comparator and applies type traits to it. MSVC requires the comparator type to be COMPLETE at the point of
  container instantiation. In your tree, KeyLess was only forward-declared / not defined before the multimap typedef/field appeared.

Fix (two lines in headers):
  1) Add a shared header that DEFINES Key and KeyLess:
       include/xindex/key_common.hpp  (in this zip)
  2) Include it BEFORE any container that names KeyLess:
       // in include/xindex/bptree_backend.hpp (top of file)
       #include "xindex/key_common.hpp"
       // in include/xindex/bpt_backend.hpp  (replace any ad-hoc KeyLess with the shared one)
       #include "xindex/key_common.hpp"

Optional cleanup:
  - Remove any duplicate/inline definitions of KeyLess scattered in headers to avoid ODR/ambiguity.
  - If you do not need a custom comparator, you may instead use std::less<Key>.


Minimal diffs to apply
----------------------

--- a/include/xindex/bptree_backend.hpp
+++ b/include/xindex/bptree_backend.hpp
@@
-#pragma once
-#include "xindex/index_backend.hpp"
-#include <map>
+#pragma once
+#include "xindex/index_backend.hpp"
+#include "xindex/key_common.hpp"   // defines xindex::Key and xindex::KeyLess
+#include <map>
+#include <vector>

@@
-using KeyMap = std::multimap<std::vector<std::uint8_t>, RecNo, KeyLess>;
+using KeyMap = std::multimap<xindex::Key, RecNo, xindex::KeyLess>;

(or simply: std::multimap<xindex::Key, RecNo>) since std::less<Key> is default-compatible.

--- a/include/xindex/bpt_backend.hpp
+++ b/include/xindex/bpt_backend.hpp
@@
-#pragma once
-#include "xindex/index_backend.hpp"
-#include <map>
+#pragma once
+#include "xindex/index_backend.hpp"
+#include "xindex/key_common.hpp"  // canonical Key + KeyLess
+#include <map>
+#include <optional>
+#include <vector>

@@
// remove any local duplicate of 'struct KeyLess { ... }' from this header


Build steps:
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  cmake --build build --config Debug


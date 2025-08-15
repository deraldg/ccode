Drop-in CLI framework files for ccode

Place the 'include' and 'src' folders into your repo root so paths match:
  include\textio.hpp
  include\predicates.hpp
  include\cli\parse.hpp
  include\cli\predicate.hpp
  include\cli\scan_options.hpp
  include\cli\scan.hpp
  src\cli\textio.cpp
  src\cli\predicates.cpp
  src\cli\parse.cpp
  src\cli\predicate.cpp

Then reconfigure + build:
  cmake -S "C:\Users\deral\code\ccode" -B "C:\Users\deral\code\ccode\build"
  cmake --build build --config Debug

Notes:
- Ensure include\textio.hpp only DECLARES unquote; the body is in src\cli\textio.cpp.
- Legacy verbs that use predicates::eval keep working via predicates.cpp/.hpp.
- New verbs can use the scan framework via include\cli\*.hpp.

#pragma once
// include/cli/expr/fn_string.hpp
//
// DotTalk++ string builtins.
// This header intentionally supports BOTH legacy "core" string helpers
// (LEFT/RIGHT/TRIM/etc.) and the newer builtin-registry contract used by
// CALC/CALCWRITE/REPLACE and fn_string_autoreg.
//
// Compatibility:
// - minArgs/maxArgs and min_args/max_args are aliases (same storage)
// - eval and fn are aliases (same storage)

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace dottalk::expr {

// ------------------------------------------------------------------
// Legacy core helpers (kept for backwards compatibility)
// ------------------------------------------------------------------
std::string LEFT  (std::string_view s, long n);
std::string RIGHT (std::string_view s, long n);
std::string LTRIM (std::string_view s);
std::string RTRIM (std::string_view s);
std::string TRIM  (std::string_view s);
std::string UPPER (std::string_view s);
std::string LOWER (std::string_view s);
std::string PROPER(std::string_view s);

inline std::string LEFT  (const std::string& s, long n) { return LEFT (std::string_view{s}, n); }
inline std::string RIGHT (const std::string& s, long n) { return RIGHT(std::string_view{s}, n); }
inline std::string LTRIM (const std::string& s)         { return LTRIM(std::string_view{s}); }
inline std::string RTRIM (const std::string& s)         { return RTRIM(std::string_view{s}); }
inline std::string TRIM  (const std::string& s)         { return TRIM (std::string_view{s}); }
inline std::string UPPER (const std::string& s)         { return UPPER(std::string_view{s}); }
inline std::string LOWER (const std::string& s)         { return LOWER(std::string_view{s}); }
inline std::string PROPER(const std::string& s)         { return PROPER(std::string_view{s}); }

// ------------------------------------------------------------------
// Builtin registry contract (used by CLI expression plumbing)
// ------------------------------------------------------------------
using BuiltinFnEval = std::string (*)(const std::vector<std::string>& argv);

#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable:4201) // nameless struct/union (intentional)
#endif
struct BuiltinFnSpec {
    const char* name;

    union {
        struct { int minArgs; int maxArgs; };
        struct { int min_args; int max_args; };
    };

    union {
        BuiltinFnEval eval;
        BuiltinFnEval fn;
    };
};

#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

const BuiltinFnSpec* string_fn_specs();
std::size_t          string_fn_specs_count();

} // namespace dottalk::expr

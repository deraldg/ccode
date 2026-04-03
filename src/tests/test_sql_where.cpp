// tests/test_where_cache_only.cpp
#include <cassert>
#include <cstdlib>
#include <string>
#include <iostream>

#include "../cli/where_eval_shared.hpp"

// NOTE: We only test compile/cache, not eval (no DbArea touch).

static void set_env_cap(const char* v) {
#if defined(_WIN32)
    _putenv_s("DOTTALK_WHERECACHE", v);
#else
    setenv("DOTTALK_WHERECACHE", v, 1);
#endif
    // Also force capacity now (env read is one-time); API clamps for us.
    where_eval::cache_set_capacity(std::strtoull(v, nullptr, 10));
}

int main() {
    using namespace where_eval;

    cache_clear();
    auto st0 = cache_stats();
    assert(st0.size == 0);

    // Capacity via env/API to 2
    set_env_cap("2");
    auto st1 = cache_stats();
    assert(st1.capacity == 2);

    // Compile two expressions ? fill cache
    auto e1 = compile_where_expr_cached("LNAME = \"A\"");
    auto e2 = compile_where_expr_cached("LNAME = \"B\"");
    auto st2 = cache_stats();
    assert(st2.size == 2);

    // Third compile ? LRU eviction (size stays 2)
    auto e3 = compile_where_expr_cached("LNAME = \"C\"");
    auto st3 = cache_stats();
    assert(st3.capacity == 2);
    assert(st3.size == 2);

    // Bump capacity to 3 ? add one more, size should grow to 3
    cache_set_capacity(3);
    (void)compile_where_expr_cached("GPA > 3");
    auto st4 = cache_stats();
    assert(st4.capacity == 3);
    assert(st4.size == 3);

    // Plan kind smoke: a ?simple? form and an ?ast? form
    auto s = compile_where_expr_cached("GENDER = \"F\"");
    auto a = compile_where_expr_cached("(GENDER = \"F\") OR (GPA >= 3.5)");
    std::cout << "simple? " << plan_kind(*s->plan) << "\n";
    std::cout << "complex? " << plan_kind(*a->plan) << "\n";
    assert((std::string)plan_kind(*s->plan) == "simple" || (std::string)plan_kind(*s->plan) == "ast");
    assert((std::string)plan_kind(*a->plan) == "simple" || (std::string)plan_kind(*a->plan) == "ast");

    // Clear
    cache_clear();
    auto st5 = cache_stats();
    assert(st5.size == 0);

    std::cout << "[OK] where cache tests passed\n";
    return 0;
}




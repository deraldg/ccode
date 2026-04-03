// /ccode/src/tests/test_where_cache_only.cpp
// Smoke tests for WHERE compile cache (no AST parser needed)
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>

#include "cli/where_eval_shared.hpp"

static void set_env_cap(const char* v) {
#if defined(_WIN32)
    _putenv_s("DOTTALK_WHERECACHE", v);
#else
    setenv("DOTTALK_WHERECACHE", v, 1);
#endif
    where_eval::cache_set_capacity(std::strtoull(v, nullptr, 10));
}

int main() {
    using namespace where_eval;

    cache_clear();
    auto s0 = cache_stats();
    assert(s0.size == 0);

    set_env_cap("2");
    auto s1 = cache_stats();
    assert(s1.capacity == 2);

    // Fill cache with SIMPLE expressions only
    (void)compile_where_expr_cached("LNAME = \"A\"");
    (void)compile_where_expr_cached("LNAME = \"B\"");
    auto s2 = cache_stats();
    assert(s2.size == 2);

    // Eviction at cap=2
    (void)compile_where_expr_cached("LNAME = \"C\"");
    auto s3 = cache_stats();
    assert(s3.capacity == 2);
    assert(s3.size == 2);

    // Grow capacity and add another SIMPLE expression
    cache_set_capacity(3);
    (void)compile_where_expr_cached("GENDER = \"F\" OR GPA >= 3.5");  // simple chain

    auto s4 = cache_stats();
    assert(s4.capacity == 3);
    assert(s4.size == 3);

    // Plan-kind smoke (still simple forms)
    auto simple1 = compile_where_expr_cached("GENDER = \"F\"");
    auto simple2 = compile_where_expr_cached("GENDER = \"F\" AND GPA > 3");
    std::cout << "k1=" << plan_kind(*simple1->plan) << "\n";
    std::cout << "k2=" << plan_kind(*simple2->plan) << "\n";
    // Either "simple" or (if normalizer changed) "ast" ? both acceptable for smoke test
    assert(std::string(plan_kind(*simple1->plan)) == "simple" ||
           std::string(plan_kind(*simple1->plan)) == "ast");
    assert(std::string(plan_kind(*simple2->plan)) == "simple" ||
           std::string(plan_kind(*simple2->plan)) == "ast");

    cache_clear();
    auto s5 = cache_stats();
    assert(s5.size == 0);

    std::cout << "[OK] where cache tests passed\n";
    return 0;
}




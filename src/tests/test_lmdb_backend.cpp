// src/tests/test_lmdb_backend.cpp
//
// Smoke test for xindex::LmdbBackend
// Verifies:
//  - open/create env directory
//  - upsert (including duplicate keys -> multiple recnos per key)
//  - seek(key) iteration
//  - scan(low, high) iteration
//  - erase(key, recno) removes only that pair
//  - persistence across close/reopen
//
// Exit code: 0 = PASS, nonzero = FAIL

#include "xindex/lmdb_backend.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

namespace fs = std::filesystem;

static int g_fail = 0;

static void CHECK(bool cond, const std::string& msg)
{
    if (!cond) {
        std::cerr << "FAIL: " << msg << "\n";
        ++g_fail;
    }
}

static xindex::Key key_from_ascii(const std::string& s)
{
    // xindex::Key is expected to be a byte vector (per our backend contract).
    xindex::Key k;
    k.resize(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        k[i] = static_cast<std::uint8_t>(s[i]);
    }
    return k;
}

static std::string key_to_ascii(const xindex::Key& k)
{
    std::string s;
    s.resize(k.size());
    for (size_t i = 0; i < k.size(); ++i) {
        s[i] = static_cast<char>(k[i]);
    }
    return s;
}

static std::vector<std::pair<std::string, std::uint64_t>>
collect_cursor(xindex::Cursor& cur)
{
    std::vector<std::pair<std::string, std::uint64_t>> out;

    xindex::Key k;
    xindex::RecNo r = 0;

    if (!cur.first(k, r)) return out;
    out.push_back({ key_to_ascii(k), static_cast<std::uint64_t>(r) });

    while (cur.next(k, r)) {
        out.push_back({ key_to_ascii(k), static_cast<std::uint64_t>(r) });
    }
    return out;
}

static bool contains_pair(
    const std::vector<std::pair<std::string, std::uint64_t>>& v,
    const std::string& k,
    std::uint64_t r)
{
    for (const auto& it : v) {
        if (it.first == k && it.second == r) return true;
    }
    return false;
}

static void dump_pairs(const char* label,
                       const std::vector<std::pair<std::string, std::uint64_t>>& v)
{
    std::cout << label << " (" << v.size() << "):\n";
    for (const auto& it : v) {
        std::cout << "  " << it.first << " -> " << it.second << "\n";
    }
}

int main()
{
    try {
        // Put smoke DB under: <repo>/dottalkpp/data/tmp/lmdb_smoke_backend
        // If you prefer build-local, change this path.
        fs::path base = fs::current_path();

        // Try to find ".../dottalkpp/data" by walking upward a bit.
        // This keeps it convenient when you run from build output directories.
        fs::path data_root;
        {
            fs::path p = base;
            for (int i = 0; i < 10; ++i) {
                fs::path cand = p / "dottalkpp" / "data";
                if (fs::exists(cand) && fs::is_directory(cand)) {
                    data_root = fs::absolute(cand);
                    break;
                }
                if (!p.has_parent_path()) break;
                p = p.parent_path();
            }
            if (data_root.empty()) {
                // Fallback: use current dir tmp
                data_root = fs::absolute(base / "tmp");
            }
        }

        fs::path env_dir = data_root / "tmp" / "lmdb_smoke_backend";
        std::cout << "LMDB smoke env dir: " << env_dir.string() << "\n";

        // Clean start
        if (fs::exists(env_dir)) {
            fs::remove_all(env_dir);
        }
        fs::create_directories(env_dir);

        // ----------------- Phase 1: open + insert + read -----------------
        {
            xindex::LmdbBackend idx;
            CHECK(idx.open(env_dir.string()), "open() should succeed");

            // Insert duplicate key A -> {1,2}
            idx.upsert(key_from_ascii("A"), static_cast<xindex::RecNo>(1));
            idx.upsert(key_from_ascii("A"), static_cast<xindex::RecNo>(2));
            idx.upsert(key_from_ascii("B"), static_cast<xindex::RecNo>(3));
            idx.upsert(key_from_ascii("C"), static_cast<xindex::RecNo>(4));

            // seek("A") should return A->1 and A->2 (order by dup value)
            {
                auto cur = idx.seek(key_from_ascii("A"));
                CHECK(static_cast<bool>(cur), "seek(A) returned cursor");

                auto rows = collect_cursor(*cur);
                dump_pairs("seek(A)", rows);

                CHECK(rows.size() >= 2, "seek(A) should yield >=2 rows (dups)");
                CHECK(contains_pair(rows, "A", 1), "seek(A) contains (A,1)");
                CHECK(contains_pair(rows, "A", 2), "seek(A) contains (A,2)");
            }

            // scan("A","B") should include A dups + B
            {
                auto cur = idx.scan(key_from_ascii("A"), key_from_ascii("B"));
                CHECK(static_cast<bool>(cur), "scan(A,B) returned cursor");

                auto rows = collect_cursor(*cur);
                dump_pairs("scan(A..B)", rows);

                CHECK(contains_pair(rows, "A", 1), "scan(A..B) contains (A,1)");
                CHECK(contains_pair(rows, "A", 2), "scan(A..B) contains (A,2)");
                CHECK(contains_pair(rows, "B", 3), "scan(A..B) contains (B,3)");
                CHECK(!contains_pair(rows, "C", 4), "scan(A..B) does NOT contain (C,4)");
            }

            // erase (A,2)
            idx.erase(key_from_ascii("A"), static_cast<xindex::RecNo>(2));

            // seek("A") should now only include (A,1)
            {
                auto cur = idx.seek(key_from_ascii("A"));
                auto rows = collect_cursor(*cur);
                dump_pairs("seek(A) after erase(A,2)", rows);

                CHECK(contains_pair(rows, "A", 1), "after erase, seek(A) contains (A,1)");
                CHECK(!contains_pair(rows, "A", 2), "after erase, seek(A) does NOT contain (A,2)");
            }

            idx.close();
        }

        // ----------------- Phase 2: reopen + persistence check -----------------
        {
            xindex::LmdbBackend idx;
            CHECK(idx.open(env_dir.string()), "reopen() should succeed");

            // A should still exist with (A,1), and (A,2) should remain erased.
            {
                auto cur = idx.seek(key_from_ascii("A"));
                auto rows = collect_cursor(*cur);
                dump_pairs("seek(A) after reopen", rows);

                CHECK(contains_pair(rows, "A", 1), "persist: seek(A) contains (A,1)");
                CHECK(!contains_pair(rows, "A", 2), "persist: seek(A) does NOT contain (A,2)");
            }

            // B and C should still exist
            {
                auto cur = idx.scan(key_from_ascii("B"), key_from_ascii("C"));
                auto rows = collect_cursor(*cur);
                dump_pairs("scan(B..C) after reopen", rows);

                CHECK(contains_pair(rows, "B", 3), "persist: contains (B,3)");
                CHECK(contains_pair(rows, "C", 4), "persist: contains (C,4)");
            }

            idx.close();
        }

        if (g_fail == 0) {
            std::cout << "PASS: LMDB backend smoke test\n";
            return 0;
        }

        std::cerr << "FAIL: LMDB backend smoke test (" << g_fail << " checks failed)\n";
        return 2;
    }
    catch (const std::exception& ex) {
        std::cerr << "EXCEPTION: " << ex.what() << "\n";
        return 3;
    }
}
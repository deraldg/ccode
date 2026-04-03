// memo_smoke.cpp
// Simple smoke test for DTX MemoStore

#include "memo/memostore.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using dottalk::memo::MemoStore;
using dottalk::memo::OpenMode;

static int fail(const std::string& msg)
{
    std::cerr << "FAIL: " << msg << "\n";
    return 1;
}

int main()
{
    try {
        const fs::path dir  = fs::path("tmp");
        const fs::path file = dir / "memo_smoke.dtx";

        std::error_code ec;
        fs::create_directories(dir, ec);
        fs::remove(file, ec);

        MemoStore store;

        {
            auto r = store.open(file.string(), OpenMode::CreateIfMissing);
            if (!r.ok) return fail("open/create failed: " + r.error);
            if (!store.is_open()) return fail("store not open after create");
        }

        auto put1 = store.put_text("alpha");
        if (!put1.ok) return fail("put_text(alpha) failed: " + put1.error);
        if (put1.ref.token.empty()) return fail("alpha ref token is empty");

        auto get1 = store.get_text(put1.ref);
        if (!get1.ok) return fail("get_text(alpha) failed: " + get1.error);
        if (get1.text != "alpha") return fail("alpha round-trip mismatch");

        auto st1 = store.stat(put1.ref);
        if (!st1.exists) return fail("stat(alpha) says object does not exist");
        if (st1.logical_bytes != 5) return fail("stat(alpha) logical_bytes mismatch");

        auto put2 = store.update_text(put1.ref, "beta");
        if (!put2.ok) return fail("update_text(beta) failed: " + put2.error);
        if (put2.ref.token.empty()) return fail("beta ref token is empty");
        if (put2.ref.token == put1.ref.token) {
            return fail("update_text returned same token; expected append-new token");
        }

        auto get2 = store.get_text(put2.ref);
        if (!get2.ok) return fail("get_text(beta) failed: " + get2.error);
        if (get2.text != "beta") return fail("beta round-trip mismatch");

        auto flush1 = store.flush();
        if (!flush1.ok) return fail("flush failed: " + flush1.error);

        store.close();
        if (store.is_open()) return fail("store still open after close");

        {
            auto r = store.open(file.string(), OpenMode::OpenExisting);
            if (!r.ok) return fail("reopen failed: " + r.error);
        }

        auto get3 = store.get_text(put1.ref);
        if (!get3.ok) return fail("reopen get_text(alpha) failed: " + get3.error);
        if (get3.text != "alpha") return fail("reopen alpha mismatch");

        auto get4 = store.get_text(put2.ref);
        if (!get4.ok) return fail("reopen get_text(beta) failed: " + get4.error);
        if (get4.text != "beta") return fail("reopen beta mismatch");

        auto erase1 = store.erase(put1.ref);
        if (!erase1.ok) return fail("erase(alpha) failed: " + erase1.error);

        auto st2 = store.stat(put1.ref);
        if (st2.exists) return fail("alpha still exists after erase");

        auto get5 = store.get_text(put2.ref);
        if (!get5.ok) return fail("beta vanished after alpha erase: " + get5.error);
        if (get5.text != "beta") return fail("beta changed after alpha erase");

        auto flush2 = store.flush();
        if (!flush2.ok) return fail("final flush failed: " + flush2.error);

        store.close();

        std::cout << "PASS: MemoStore smoke test passed\n";
        std::cout << "  file: " << file.string() << "\n";
        std::cout << "  ref1: " << put1.ref.token << "\n";
        std::cout << "  ref2: " << put2.ref.token << "\n";
        return 0;
    }
    catch (const std::exception& ex) {
        return fail(std::string("exception: ") + ex.what());
    }
}
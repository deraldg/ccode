#include "xbase_locks.hpp"
#include "xbase.hpp"

#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <string>
#include <sstream>

#ifdef _WIN32
  #include <windows.h>
  #include <lmcons.h>
#else
  #include <unistd.h>
  #include <sys/utsname.h>
  #include <signal.h>
  #include <cerrno>
#endif

namespace fs = std::filesystem;

namespace xbase::locks {

// ---------------- process/session owner --------------------------------------
static std::string make_owner_string() {
#ifdef _WIN32
    char cname[MAX_COMPUTERNAME_LENGTH + 1]{0};
    DWORD sz = MAX_COMPUTERNAME_LENGTH + 1;
    std::string host;
    if (::GetComputerNameA(cname, &sz)) host.assign(cname, sz); else host = "winhost";
    const unsigned long pid = static_cast<unsigned long>(::GetCurrentProcessId());
#else
    utsname u{};
    std::string host;
    if (::uname(&u) == 0) host = u.nodename; else host = "unixhost";
    const unsigned long pid = static_cast<unsigned long>(::getpid());
#endif

    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

    std::ostringstream os;
    os << host << ":" << pid << ":" << ms;
    return os.str();
}

const Owner& current_owner() {
    static Owner g_owner{ make_owner_string() };
    return g_owner;
}

// ---------------- in-memory bookkeeping (this process only) -------------------
struct LockBook {
    bool table{false};
    std::unordered_set<uint32_t> recs;
};

static std::unordered_map<const DbArea*, LockBook>& book() {
    static std::unordered_map<const DbArea*, LockBook> g;
    return g;
}

// ---------------- paths -------------------------------------------------------
static std::string resolved_db_path(const DbArea& a) {
    return a.filename();
}

static std::string table_lock_path(const DbArea& a) {
    fs::path p = resolved_db_path(a);
    p += ".lock";
    return p.string();
}

static std::string record_lock_path(const DbArea& a, uint32_t recno) {
    fs::path p = resolved_db_path(a);
    p += ".lock.";
    p += std::to_string(recno);
    return p.string();
}

// ---------------- lock file metadata -----------------------------------------
struct LockMeta {
    std::string owner;
    unsigned long pid{0};
    long long ms{0};
};

static bool read_lock_meta(const std::string& path, LockMeta& meta) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("owner=", 0) == 0) {
            meta.owner = line.substr(6);
        } else if (line.rfind("pid=", 0) == 0) {
            try {
                meta.pid = static_cast<unsigned long>(std::stoul(line.substr(4)));
            } catch (...) {
                meta.pid = 0;
            }
        } else if (line.rfind("ms=", 0) == 0) {
            try {
                meta.ms = std::stoll(line.substr(3));
            } catch (...) {
                meta.ms = 0;
            }
        }
    }

    return !meta.owner.empty();
}

static bool read_owner_from_file(const std::string& path, std::string& out_owner) {
    LockMeta meta;
    if (!read_lock_meta(path, meta)) return false;
    out_owner = meta.owner;
    return true;
}

static bool write_lock_file(const std::string& path, const Owner& owner, std::string* err) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) {
        if (err) *err = "cannot create lock";
        return false;
    }

#ifdef _WIN32
    const unsigned long pid = static_cast<unsigned long>(::GetCurrentProcessId());
#else
    const unsigned long pid = static_cast<unsigned long>(::getpid());
#endif

    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

    f << "DotTalk++ lock\n";
    f << "owner=" << owner.id << "\n";
    f << "pid="   << pid      << "\n";
    f << "ms="    << ms       << "\n";
    f.flush();

    const bool ok = static_cast<bool>(f);
    if (!ok && err) *err = "write failed";
    return ok;
}

static bool force_remove(const std::string& path, std::string* err) {
    std::error_code ec;
    fs::remove(path, ec);
    if (ec && fs::exists(path)) {
        if (err) *err = "force unlock failed";
        return false;
    }
    return true;
}

static bool is_pid_alive(unsigned long pid) {
    if (pid == 0) return false;

#ifdef _WIN32
    HANDLE h = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!h) return false;

    DWORD exit_code = 0;
    const BOOL ok = ::GetExitCodeProcess(h, &exit_code);
    ::CloseHandle(h);

    if (!ok) return false;
    return exit_code == STILL_ACTIVE;
#else
    const int rc = ::kill(static_cast<pid_t>(pid), 0);
    if (rc == 0) return true;
    if (errno == EPERM) return true;   // exists, but no permission
    return false;
#endif
}

static bool remove_if_owned(const std::string& path, const Owner& me, std::string* err) {
    if (!fs::exists(path)) return true;

    LockMeta meta;
    if (!read_lock_meta(path, meta)) {
        if (err) *err = "unknown owner";
        return false;
    }

    if (meta.owner != me.id) {
        if (err) *err = "not lock owner";
        return false;
    }

    std::error_code ec;
    fs::remove(path, ec);
    if (ec && fs::exists(path)) {
        if (err) *err = "unlock failed";
        return false;
    }
    return true;
}

static bool create_or_validate_owned(const std::string& path, const Owner& me, std::string* err) {
    if (!fs::exists(path)) {
        return write_lock_file(path, me, err);
    }

    LockMeta meta;
    if (!read_lock_meta(path, meta)) {
        if (err) *err = "lock exists";
        return false;
    }

    // Re-entrant lock in same process/session.
    if (meta.owner == me.id) {
        return true;
    }

    // Stale lock: owner process is gone.
    if (!is_pid_alive(meta.pid)) {
        std::string ignored;
        if (!force_remove(path, &ignored)) {
            if (err) *err = "stale lock exists";
            return false;
        }
        return write_lock_file(path, me, err);
    }

    if (err) *err = "lock exists";
    return false;
}

// ---------------- Public API: Table ------------------------------------------

bool try_lock_table(DbArea& a, const Owner& me, std::string* err) {
    const std::string lp = table_lock_path(a);
    if (!create_or_validate_owned(lp, me, err)) return false;
    book()[&a].table = true;
    return true;
}

bool unlock_table(DbArea& a, const Owner& me, std::string* err) {
    const std::string lp = table_lock_path(a);
    const bool ok = remove_if_owned(lp, me, err);
    if (ok) book()[&a].table = false;
    return ok;
}

bool is_table_locked(const DbArea& a, std::string* owner_out) {
    const std::string lp = table_lock_path(a);
    if (!fs::exists(lp)) return false;

    if (owner_out) {
        LockMeta meta;
        if (read_lock_meta(lp, meta)) *owner_out = meta.owner;
        else *owner_out = "";
    }
    return true;
}

// Back-compat shims
bool try_lock_table(DbArea& a, std::string* err) {
    return try_lock_table(a, current_owner(), err);
}

void unlock_table(DbArea& a) {
    std::string ignored;
    (void)unlock_table(a, current_owner(), &ignored);
}

bool is_table_locked(const DbArea& a) {
    return fs::exists(table_lock_path(a));
}

// ---------------- Public API: Record -----------------------------------------

bool try_lock_record(DbArea& a, uint32_t recno, const Owner& me, std::string* err) {
    if (recno == 0) {
        if (err) *err = "bad recno";
        return false;
    }

    // If table is locked by someone else and still alive, deny.
    const std::string tlp = table_lock_path(a);
    if (fs::exists(tlp)) {
        LockMeta tmeta;
        if (read_lock_meta(tlp, tmeta)) {
            if (tmeta.owner != me.id && is_pid_alive(tmeta.pid)) {
                if (err) *err = "table locked";
                return false;
            }
            // stale table lock cleanup
            if (tmeta.owner != me.id && !is_pid_alive(tmeta.pid)) {
                std::string ignored;
                (void)force_remove(tlp, &ignored);
            }
        } else {
            if (err) *err = "table locked";
            return false;
        }
    }

    const std::string rp = record_lock_path(a, recno);
    if (!create_or_validate_owned(rp, me, err)) return false;

    book()[&a].recs.insert(recno);
    return true;
}

bool unlock_record(DbArea& a, uint32_t recno, const Owner& me, std::string* err) {
    if (recno == 0) {
        if (err) *err = "bad recno";
        return false;
    }

    const std::string rp = record_lock_path(a, recno);
    const bool ok = remove_if_owned(rp, me, err);
    if (ok) book()[&a].recs.erase(recno);
    return ok;
}

bool is_record_locked(const DbArea& a, uint32_t recno, std::string* owner_out) {
    if (recno == 0) return false;

    const std::string rp = record_lock_path(a, recno);
    if (!fs::exists(rp)) return false;

    if (owner_out) {
        LockMeta meta;
        if (read_lock_meta(rp, meta)) *owner_out = meta.owner;
        else *owner_out = "";
    }
    return true;
}

// Back-compat shims
bool try_lock_record(DbArea& a, uint32_t recno, std::string* err) {
    return try_lock_record(a, recno, current_owner(), err);
}

void unlock_record(DbArea& a, uint32_t recno) {
    std::string ignored;
    (void)unlock_record(a, recno, current_owner(), &ignored);
}

bool is_record_locked(const DbArea& a, uint32_t recno) {
    return fs::exists(record_lock_path(a, recno));
}

// ---------------- Admin -------------------------------------------------------

bool force_unlock_table(DbArea& a, std::string* err) {
    const std::string lp = table_lock_path(a);
    const bool ok = force_remove(lp, err);
    if (ok) book()[&a].table = false;
    return ok;
}

bool force_unlock_record(DbArea& a, uint32_t recno, std::string* err) {
    if (recno == 0) {
        if (err) *err = "bad recno";
        return false;
    }

    const std::string rp = record_lock_path(a, recno);
    const bool ok = force_remove(rp, err);
    if (ok) book()[&a].recs.erase(recno);
    return ok;
}

// ---------------- Cleanup -----------------------------------------------------

void release_held(DbArea& a) {
    const Owner& me = current_owner();
    auto it = book().find(&a);
    if (it == book().end()) return;

    for (auto r : it->second.recs) {
        std::string ignored;
        (void)unlock_record(a, r, me, &ignored);
    }
    it->second.recs.clear();

    if (it->second.table) {
        std::string ignored;
        (void)unlock_table(a, me, &ignored);
        it->second.table = false;
    }
}

} // namespace xbase::locks
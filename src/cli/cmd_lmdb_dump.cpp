// File: src/cli/cmd_lmdb_dump.cpp
//
// Command:
//   LMDBDUMP <env_path>
//       [--db <name>] [-db <name>]
//       [--grep <ASCII>] [-grep <ASCII>]
//       [--trydb]
//       [--limit <N>]
//       [--start <key>]        (ASCII unless prefixed with 0x...)
//       [--starthex <hex...>]  (hex bytes; spaces OK)
//
// Examples:
//   LMDBDUMP indexes\students.cdx.d
//   LMDBDUMP indexes\students.cdx.d --trydb
//   LMDBDUMP indexes\students.cdx.d --grep MILLER --limit 50
//   LMDBDUMP indexes\students.cdx.d --db lname --start M --limit 200
//   LMDBDUMP indexes\students.cdx.d --starthex 4d 49 4c 4c 45 52
//
// Notes:
// - Standalone: opens LMDB env read-only from <env_path>. Does NOT depend on your xindex backend.
// - Wire from your existing cmd_LMDB dispatcher (DO NOT define cmd_LMDB here).

#include <lmdb.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace xbase {
class DbArea;
}  // namespace xbase

namespace {

struct LmdbError final : std::runtime_error {
  using std::runtime_error::runtime_error;
};

void check(int rc, const char* what) {
  if (rc != MDB_SUCCESS) {
    std::ostringstream oss;
    oss << what << ": " << mdb_strerror(rc) << " (rc=" << rc << ")";
    throw LmdbError(oss.str());
  }
}

std::string hex_preview(const void* data, size_t len, size_t max_bytes = 64) {
  const auto* p = static_cast<const uint8_t*>(data);
  const size_t n = (len < max_bytes) ? len : max_bytes;

  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (size_t i = 0; i < n; ++i) {
    oss << std::setw(2) << static_cast<unsigned>(p[i]);
    if (i + 1 != n) oss << ' ';
  }
  if (len > n) oss << " ...";
  return oss.str();
}

std::string ascii_preview(const void* data, size_t len, size_t max_bytes = 128) {
  const auto* p = static_cast<const uint8_t*>(data);
  const size_t n = (len < max_bytes) ? len : max_bytes;

  std::string s;
  s.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    const unsigned char c = static_cast<unsigned char>(p[i]);
    if (c == 0)
      s.push_back('.');
    else if (std::isprint(c))
      s.push_back(static_cast<char>(c));
    else
      s.push_back('.');
  }
  if (len > n) s += " ...";
  return s;
}

bool mem_contains_ascii(const void* data, size_t len, const std::string& needle) {
  if (needle.empty()) return false;
  if (needle.size() > len) return false;

  const auto* p = static_cast<const uint8_t*>(data);
  const auto* n = reinterpret_cast<const uint8_t*>(needle.data());
  const size_t nlen = needle.size();

  for (size_t i = 0; i + nlen <= len; ++i) {
    if (std::memcmp(p + i, n, nlen) == 0) return true;
  }
  return false;
}

static bool is_hex_digit(char c) {
  return std::isdigit(static_cast<unsigned char>(c)) || (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
}

static int hex_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

std::vector<uint8_t> parse_hex_bytes(std::string_view s) {
  std::vector<uint8_t> out;
  int hi = -1;

  for (char c : s) {
    if (!is_hex_digit(c)) continue;
    int v = hex_value(c);
    if (hi < 0) {
      hi = v;
    } else {
      out.push_back(static_cast<uint8_t>((hi << 4) | v));
      hi = -1;
    }
  }
  if (hi >= 0) throw LmdbError("Odd number of hex digits in --starthex");
  return out;
}

std::vector<uint8_t> parse_start_key(const std::string& s) {
  if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
    return parse_hex_bytes(std::string_view{s}.substr(2)); // <-- strip 0x
  }
  return std::vector<uint8_t>(s.begin(), s.end());
}

uint64_t parse_u64(const std::string& s, const char* what) {
  if (s.empty()) throw LmdbError(std::string("Missing value for ") + what);
  char* end = nullptr;
  errno = 0;
  unsigned long long v = std::strtoull(s.c_str(), &end, 10);
  if (errno != 0 || end == s.c_str() || *end != '\0') {
    throw LmdbError(std::string("Invalid number for ") + what + ": " + s);
  }
  return static_cast<uint64_t>(v);
}

bool is_option_token(const std::string& t) {
  return !t.empty() && t[0] == '-';
}

std::vector<std::string> tokenize(std::istringstream& iss) {
  std::vector<std::string> out;
  std::string t;
  while (iss >> t) out.push_back(t);
  return out;
}

std::string join_tokens(const std::vector<std::string>& v, size_t from, size_t to_excl) {
  std::string s;
  for (size_t i = from; i < to_excl; ++i) {
    if (!s.empty()) s.push_back(' ');
    s += v[i];
  }
  return s;
}

struct DumpArgs {
  std::string env_path;  // required
  std::optional<std::string> db_name;
  std::optional<std::string> grep;

  bool help = false;
  bool trydb = false;

  uint64_t limit = std::numeric_limits<uint64_t>::max();
  std::optional<std::vector<uint8_t>> start_key;
};

void print_dump_help() {
  std::cout
      << "Usage:\n"
      << "  LMDBDUMP <env_path>\n"
      << "      [--db <name>] [-db <name>]\n"
      << "      [--grep <ASCII>] [-grep <ASCII>]\n"
      << "      [--trydb]\n"
      << "      [--limit <N>]\n"
      << "      [--start <key>]\n"
      << "      [--starthex <hex...>]\n"
      << "\n"
      << "Notes:\n"
      << "  --start treats <key> as ASCII unless it begins with 0x (hex).\n"
      << "  --starthex accepts bytes as hex, spaces allowed.\n"
      << "\n"
      << "Examples:\n"
      << "  LMDBDUMP indexes\\students.cdx.d\n"
      << "  LMDBDUMP indexes\\students.cdx.d --trydb\n"
      << "  LMDBDUMP indexes\\students.cdx.d --grep MILLER --limit 50\n"
      << "  LMDBDUMP indexes\\students.cdx.d --db lname --start M --limit 200\n"
      << "  LMDBDUMP indexes\\students.cdx.d --starthex 4d 49 4c 4c 45 52\n";
}

DumpArgs parse_dump_args(std::istringstream& iss) {
  DumpArgs a;

  if (!(iss >> a.env_path) || a.env_path.empty()) {
    throw LmdbError("Missing <env_path>");
  }

  const auto toks = tokenize(iss);

  auto need = [&](size_t& i, const char* flag) -> const std::string& {
    if (i + 1 >= toks.size()) throw LmdbError(std::string(flag) + " requires a value");
    return toks[++i];
  };

  for (size_t i = 0; i < toks.size(); ++i) {
    const std::string& tok = toks[i];

    if (tok == "--db" || tok == "-db") {
      a.db_name = need(i, "--db");
    } else if (tok == "--grep" || tok == "-grep") {
      a.grep = need(i, "--grep");
    } else if (tok == "--trydb") {
      a.trydb = true;
    } else if (tok == "--limit") {
      a.limit = parse_u64(need(i, "--limit"), "--limit");
    } else if (tok == "--start") {
      a.start_key = parse_start_key(need(i, "--start"));
    } else if (tok == "--starthex") {
      // Consume ALL following non-option tokens as hex bytes: "--starthex 4d 49 4c ..."
      if (i + 1 >= toks.size()) throw LmdbError("--starthex requires a value");
      size_t j = i + 1;
      while (j < toks.size() && !is_option_token(toks[j])) ++j;
      const std::string hex = join_tokens(toks, i + 1, j);
      a.start_key = parse_hex_bytes(hex);
      i = j - 1;
    } else if (tok == "--help" || tok == "-h") {
      a.help = true;
    } else {
      throw LmdbError("Unknown option: " + tok);
    }
  }

  return a;
}

[[noreturn]] void throw_with_tip_named_db_not_found(const std::string& name) {
  std::ostringstream oss;
  oss << "LMDBDUMP: named DB not found: '" << name << "'.\n"
      << "Tip: this env may have no named DBs (only the main DB), or names differ (e.g. lname/tag_lname).\n"
      << "Try: LMDBDUMP <env_path> --trydb\n";
  throw LmdbError(oss.str());
}

struct EnvGuard {
  MDB_env* env = nullptr;
  ~EnvGuard() { if (env) mdb_env_close(env); }
};

struct TxnGuard {
  MDB_txn* txn = nullptr;
  ~TxnGuard() { if (txn) mdb_txn_abort(txn); }
};

struct CursorGuard {
  MDB_cursor* cur = nullptr;
  ~CursorGuard() { if (cur) mdb_cursor_close(cur); }
};

void dump_one_db(MDB_env* env,
                 const std::optional<std::string>& db_name,
                 const std::optional<std::string>& grep,
                 uint64_t limit,
                 const std::optional<std::vector<uint8_t>>& start_key) {
  TxnGuard tg;
  check(mdb_txn_begin(env, nullptr, MDB_RDONLY, &tg.txn), "mdb_txn_begin");

  auto open_dbi_with_retry = [&](const char* name, MDB_dbi* out) -> int {
    // IMPORTANT:
    //   Our tag DBs are created with MDB_DUPSORT|MDB_DUPFIXED.
    //   Passing 0 here can yield MDB_INCOMPATIBLE even when the DB exists.
    int rc0 = mdb_dbi_open(tg.txn, name, 0, out);
    if ((rc0 == MDB_INCOMPATIBLE || rc0 == MDB_NOTFOUND) && name != nullptr) {
      // Some builds may return MDB_NOTFOUND (not MDB_INCOMPATIBLE) if the DB exists but flags mismatch.
      int rc1 = mdb_dbi_open(tg.txn, name, MDB_DUPSORT | MDB_DUPFIXED, out);
      return rc1;
    }
    return rc0;
  };

  MDB_dbi dbi{};
  const char* name = db_name ? db_name->c_str() : nullptr;
  int rc = open_dbi_with_retry(name, &dbi);
  if ((rc == MDB_NOTFOUND || rc == MDB_INCOMPATIBLE) && db_name) {
    // "NOTFOUND" means the named DB key isn't present.
    // "INCOMPATIBLE" most commonly means it *is* present, but flags mismatch.
    // We already retried with our expected flags; if it still fails, surface a clear message.
    std::ostringstream oss;
    oss << "LMDBDUMP: failed to open named DB '" << *db_name << "': "
        << mdb_strerror(rc) << " (rc=" << rc << ")\n"
        << "Tip: try 'LMDBDUMP <env_path> --trydb' to see what names/flags are present.\n";
    throw LmdbError(oss.str());
  }
  check(rc, "mdb_dbi_open");

  CursorGuard cg;
  check(mdb_cursor_open(tg.txn, dbi, &cg.cur), "mdb_cursor_open");

  MDB_val key{}, val{};
  if (start_key && !start_key->empty()) {
    key.mv_size = start_key->size();
    key.mv_data = const_cast<uint8_t*>(start_key->data());
    rc = mdb_cursor_get(cg.cur, &key, &val, MDB_SET_RANGE);
  } else {
    rc = mdb_cursor_get(cg.cur, &key, &val, MDB_FIRST);
  }

  uint64_t count = 0, matched = 0, printed = 0;
  while (rc == MDB_SUCCESS) {
    ++count;

    bool ok = true;
    if (grep && !grep->empty()) ok = mem_contains_ascii(val.mv_data, val.mv_size, *grep);

    if (ok) {
      ++matched;
      if (printed < limit) {
        ++printed;
        std::cout << "=== record " << count << " ===\n";
        std::cout << "key.size=" << key.mv_size << "  val.size=" << val.mv_size << "\n";
        std::cout << "key.hex : " << hex_preview(key.mv_data, key.mv_size) << "\n";
        std::cout << "key.asci: " << ascii_preview(key.mv_data, key.mv_size) << "\n";
        std::cout << "val.hex : " << hex_preview(val.mv_data, val.mv_size) << "\n";
        std::cout << "val.asci: " << ascii_preview(val.mv_data, val.mv_size) << "\n";
      } else {
        break;
      }
    }

    rc = mdb_cursor_get(cg.cur, &key, &val, MDB_NEXT);
  }
  if (rc != MDB_NOTFOUND && rc != MDB_SUCCESS) check(rc, "mdb_cursor_get");

  std::cout << "Scanned: " << count << " record(s)\n";
  if (grep && !grep->empty()) std::cout << "Matched: " << matched << " record(s)\n";
  if (limit != std::numeric_limits<uint64_t>::max()) {
    std::cout << "Printed: " << printed << " record(s) (limit)\n";
  }

  mdb_dbi_close(env, dbi);
}

std::string to_lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
std::string to_upper(std::string s) {
  for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}

void trydb(MDB_env* env) {
  // Iterate main DB keys and try them (and lower/upper variants) as named DBs.
  TxnGuard tg;
  check(mdb_txn_begin(env, nullptr, MDB_RDONLY, &tg.txn), "mdb_txn_begin");

  MDB_dbi main_dbi{};
  check(mdb_dbi_open(tg.txn, nullptr, 0, &main_dbi), "mdb_dbi_open(main)");

  CursorGuard cg;
  check(mdb_cursor_open(tg.txn, main_dbi, &cg.cur), "mdb_cursor_open(main)");

  MDB_val k{}, v{};
  int rc = mdb_cursor_get(cg.cur, &k, &v, MDB_FIRST);

  std::cout << "TRYDB: scanning main DB keys and probing named DBs...\n";

  uint64_t keys = 0, found = 0;
  while (rc == MDB_SUCCESS) {
    ++keys;

    std::string raw(static_cast<const char*>(k.mv_data),
                    static_cast<const char*>(k.mv_data) + k.mv_size);

    std::vector<std::string> candidates;
    candidates.push_back(raw);
    candidates.push_back(to_lower(raw));
    candidates.push_back(to_upper(raw));

    // de-dup
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

    for (const auto& name : candidates) {
      MDB_dbi probe{};
      int prc = mdb_dbi_open(tg.txn, name.c_str(), 0, &probe);
      if (prc == MDB_INCOMPATIBLE) {
        // Our tag DBs are DUPSORT+DUPFIXED. Retry with those flags.
        prc = mdb_dbi_open(tg.txn, name.c_str(), MDB_DUPSORT | MDB_DUPFIXED, &probe);
      }
      if (prc == MDB_SUCCESS) {
        ++found;
        MDB_stat st{};
        check(mdb_stat(tg.txn, probe, &st), "mdb_stat");
        unsigned int flags = 0;
        (void)mdb_dbi_flags(tg.txn, probe, &flags);
        std::cout << "  + DB '" << name << "' exists (entries=" << st.ms_entries << ")";
        if (flags != 0) {
          std::cout << " flags=" << std::hex << flags << std::dec;
        }
        std::cout << "\n";
        mdb_dbi_close(env, probe);
      } else if (prc != MDB_NOTFOUND) {
        // Surface other errors; this is a debug/inspection tool.
        std::cout << "  - probe '" << name << "' failed: " << mdb_strerror(prc)
                  << " (rc=" << prc << ")\n";
      }
    }

    rc = mdb_cursor_get(cg.cur, &k, &v, MDB_NEXT);
  }
  if (rc != MDB_NOTFOUND) check(rc, "mdb_cursor_get(main)");

  std::cout << "TRYDB: keys scanned=" << keys << "  DBs found=" << found << "\n";
  mdb_dbi_close(env, main_dbi);
}

void do_dump(const DumpArgs& args) {
  EnvGuard eg;
  check(mdb_env_create(&eg.env), "mdb_env_create");
  check(mdb_env_set_maxdbs(eg.env, 256), "mdb_env_set_maxdbs");
  check(mdb_env_open(eg.env, args.env_path.c_str(), MDB_RDONLY, 0664), "mdb_env_open");

  if (args.trydb) trydb(eg.env);
  dump_one_db(eg.env, args.db_name, args.grep, args.limit, args.start_key);
}

}  // namespace

// Called by your existing cmd_LMDB dispatcher when sub == "DUMP".
void cmd_LMDB_DUMP(xbase::DbArea& /*A*/, std::istringstream& iss) {
  try {
    DumpArgs args = parse_dump_args(iss);
    if (args.help) {
      print_dump_help();
      return;
    }
    do_dump(args);
  } catch (const std::exception& e) {
    std::cout << e.what() << "\n";
    print_dump_help();
  }
}
// src/cli/command_registry.cpp
#include "command_registry.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>      // fprintf
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

// Intentional singletons (leaked) to avoid shutdown order issues.
using Map   = std::unordered_map<std::string, cli::Handler>;
using Names = std::vector<std::string>;
using Mtx   = std::mutex;

Map&   commands() { static auto* p = new Map();   return *p; }
Names& names()    { static auto* p = new Names(); return *p; }
Mtx&   mutex_()   { static auto* p = new Mtx();   return *p; }

std::string upcopy(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
  return s;
}

std::string trim_left(std::string s) {
  const auto p = s.find_first_not_of(" \t");
  if (p != std::string::npos) s.erase(0, p);
  else s.clear();
  return s;
}

// Split into tokens with simple quote handling (supports "..." or '...')
std::vector<std::string> split_tokens(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  bool inq = false;
  char q = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (inq) {
      if (c == q) { inq = false; }
      else        { cur.push_back(c); }
    } else {
      if (c == '"' || c == '\'') { inq = true; q = c; }
      else if (std::isspace(static_cast<unsigned char>(c))) {
        if (!cur.empty()) { out.push_back(cur); cur.clear(); }
      } else {
        cur.push_back(c);
      }
    }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

inline bool is_numeric_like(const std::string& s) {
  // allow leading +/- and digits, optional single dot
  bool seenDigit = false, seenDot = false;
  size_t i = 0;
  if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
  for (; i < s.size(); ++i) {
    const char c = s[i];
    if (std::isdigit(static_cast<unsigned char>(c))) { seenDigit = true; continue; }
    if (c == '.' && !seenDot) { seenDot = true; continue; }
    return false;
  }
  return seenDigit;
}

} // anonymous namespace

namespace cli {

CommandRegistry& registry() {
  static auto* p = new CommandRegistry(); // leaked on purpose
  return *p;
}

void CommandRegistry::add(const std::string& name, Handler h) {
  const auto key = upcopy(name);
  std::lock_guard<Mtx> lock(mutex_());

  commands()[key] = std::move(h);

  auto& ns = names();
  if (std::find(ns.begin(), ns.end(), key) == ns.end())
    ns.push_back(key);
}

// Trace every command: echo raw input, then parser view, to STDERR.
// Also:
//  - FIND fallback: if user typed FIND <field> <value>, but only LOCATE exists,
//    map to "LOCATE FOR <field> = <value>" (quoted if needed).
//  - LOCATE fallback: if user typed LOCATE (FOR) <field> (=) <value>, try to
//    ensure an index exists by calling INDEX ON <field>, then call FIND.
//    (This makes FIND work even when the user hasn't indexed yet.)
bool CommandRegistry::run(xbase::DbArea& area,
                          const std::string& name,
                          std::istringstream& args) const
{
  const auto key = upcopy(name);
  std::lock_guard<Mtx> lock(mutex_());

  auto it = commands().find(key);
  if (it == commands().end())
    return false;

  // ---- Compute remaining args (non-destructive) ----
  std::string rem;
  try {
    const std::streampos pos = args.tellg();
    const std::string all = args.str();
    rem = (pos != std::streampos(-1) && static_cast<size_t>(pos) < all.size())
            ? all.substr(static_cast<size_t>(pos))
            : all;
    rem = trim_left(rem);

    // If first token equals the verb itself (case-insensitive), strip it
    if (!rem.empty()) {
      size_t tok_end = rem.find_first_of(" \t");
      const std::string firstTok = (tok_end == std::string::npos) ? rem : rem.substr(0, tok_end);
      if (upcopy(firstTok) == key) {
        rem = (tok_end == std::string::npos) ? std::string() : trim_left(rem.substr(tok_end));
      }
    }
  } catch (...) {
    rem.clear();
  }

  // ---- Trace (stderr) ----
  try {
    if (!rem.empty()) std::fprintf(stderr, "[input] %s %s\n", name.c_str(), rem.c_str());
    else              std::fprintf(stderr, "[input] %s\n", name.c_str());

    if (!rem.empty())
      std::fprintf(stderr, "[parse] CMD=%s ARGS=\"%s\"\n", key.c_str(), rem.c_str());
    else
      std::fprintf(stderr, "[parse] CMD=%s\n", key.c_str());
  } catch (...) {
    // best-effort
  }

  // ---------------------- Smart fallbacks ----------------------

  // 1) FIND fallback -> LOCATE FOR ...
  if (key == "FIND") {
    auto loc = commands().find("LOCATE");
    if (loc != commands().end()) {
      // Accept: "<field> <value>" or "<field> = <value>"
      auto toks = split_tokens(rem);
      if (toks.size() >= 2) {
        std::string field = toks[0];
        std::string value;
        if (toks.size() >= 3 && (toks[1] == "=" || toks[1] == "==" )) {
          for (size_t i = 2; i < toks.size(); ++i) {
            if (i > 2) value.push_back(' ');
            value += toks[i];
          }
        } else {
          for (size_t i = 1; i < toks.size(); ++i) {
            if (i > 1) value.push_back(' ');
            value += toks[i];
          }
        }
        // Quote non-numeric values if not already quoted
        bool quoted = (!value.empty() && ((value.front() == '"' && value.back() == '"')
                                       || (value.front() == '\'' && value.back() == '\'')));
        if (!quoted && !is_numeric_like(value)) {
          value = "\"" + value + "\"";
        }

        std::string locateArgs = "FOR " + field + " = " + value;
        std::istringstream ss(locateArgs);
        loc->second(area, ss);
        return true;
      }
    }
    // fall through to the native FIND if we couldn't rewrite
  }

  // 2) LOCATE fallback -> (ensure index) -> FIND
  if (key == "LOCATE") {
    auto idx = commands().find("INDEX");
    auto fnd = commands().find("FIND");
    if (fnd != commands().end()) {
      // Accept: "FOR <field> = <value>" OR "<field> = <value>" OR "<field> <value>"
      std::string expr = rem;
      // Strip optional leading "FOR"
      if (expr.size() >= 3 && upcopy(expr.substr(0,3)) == "FOR") {
        expr = trim_left(expr.substr(3));
      }

      auto toks = split_tokens(expr);
      if (toks.size() >= 2) {
        std::string field = toks[0];
        std::string value;
        size_t start = 1;
        if (toks.size() >= 3 && (toks[1] == "=" || toks[1] == "==" )) {
          start = 2;
        }
        for (size_t i = start; i < toks.size(); ++i) {
          if (i > start) value.push_back(' ');
          value += toks[i];
        }
        // Quote non-numeric values if not already quoted
        bool quoted = (!value.empty() && ((value.front() == '"' && value.back() == '"')
                                       || (value.front() == '\'' && value.back() == '\'')));
        if (!quoted && !is_numeric_like(value)) {
          value = "\"" + value + "\"";
        }

        // If INDEX exists, try to ensure an index on this field (best-effort)
        if (idx != commands().end()) {
          std::string indexArgs = "ON " + field;
          std::istringstream si(indexArgs);
          try { idx->second(area, si); } catch (...) { /* ignore */ }
        }

        // Now call FIND <field> <value>
        std::string findArgs = field + " " + value;
        std::istringstream sf(findArgs);
        fnd->second(area, sf);
        return true;
      }
    }
    // fall through to native LOCATE if we couldn't rewrite
  }

  // ---------------------- Execute native handler ----------------------
  it->second(area, args);
  return true;
}

const std::vector<std::string>& CommandRegistry::list() const {
  return names();
}

} // namespace cli

// ---------------- Back-compat free-function shims ----------------
namespace command_registry {
  void register_command(const std::string& name, cli::Handler h) {
    cli::registry().add(name, std::move(h));
  }
  const std::unordered_map<std::string, cli::Handler>& map() {
    return commands();
  }
  const std::vector<std::string>& list_names() {
    return names();
  }
} // namespace command_registry

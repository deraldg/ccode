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

// Split into tokens with simple quote handling (supports "..." or '...').
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

// Trace every command to stderr and support smart fallbacks.
//  - FIND <field> [=] <value>          -> LOCATE FOR <field> = <value>  (only if native FIND missing)
//  - LOCATE (FOR) <field> (=) <value>  -> ensure INDEX (best-effort) then FIND <field> <value> (only if native LOCATE missing)
bool CommandRegistry::run(xbase::DbArea& area,
                          const std::string& name,
                          std::istringstream& args) const
{
  const std::string key = upcopy(name);

  // Compute remaining args (non-destructive).
  std::string rem;
  try {
    std::streampos pos = args.tellg();
    if (pos == std::streampos(-1)) pos = 0;
    const std::string all = args.str();
    rem = (static_cast<size_t>(pos) < all.size()) ? all.substr(static_cast<size_t>(pos)) : std::string();
    rem = trim_left(rem);

    // If first token equals the verb itself, strip it (defensive).
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

  // Copy out needed handlers under the mutex, then release before invoking any.
  Handler mainH;
  Handler locateH; // for FIND fallback
  Handler findH;   // for LOCATE fallback
  Handler indexH;  // optional: ensure index before FIND
  bool haveMain = false;

  {
    std::lock_guard<Mtx> lock(mutex_());

    auto it = commands().find(key);
    if (it != commands().end()) { mainH = it->second; haveMain = true; }

    auto loc = commands().find("LOCATE");
    if (loc != commands().end()) locateH = loc->second;

    auto fnd = commands().find("FIND");
    if (fnd != commands().end()) findH = fnd->second;

    auto idx = commands().find("INDEX");
    if (idx != commands().end()) indexH = idx->second;
  } // mutex released here

  if (!haveMain && key != "FIND" && key != "LOCATE") return false; // no handler & no special casing

  // Trace (stderr).
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

  // -------- Smart fallbacks (callbacks happen with no lock held) --------

  // 1) FIND fallback -> LOCATE FOR <field> = <value> (only if native FIND missing)
  if (key == "FIND" && !haveMain && locateH) {
    auto toks = split_tokens(rem);
    if (toks.size() >= 2) {
      std::string field = toks[0];
      std::string value;
      if (toks.size() >= 3 && (toks[1] == "=" || toks[1] == "==")) {
        for (size_t i = 2; i < toks.size(); ++i) { if (i > 2) value.push_back(' '); value += toks[i]; }
      } else {
        for (size_t i = 1; i < toks.size(); ++i) { if (i > 1) value.push_back(' '); value += toks[i]; }
      }
      bool quoted = (!value.empty() &&
                     ((value.front()=='"' && value.back()=='"') ||
                      (value.front()=='\'' && value.back()=='\'')));
      if (!quoted && !is_numeric_like(value)) value = "\"" + value + "\"";

      std::string locateArgs = "FOR " + field + " = " + value;
      std::istringstream ss(locateArgs);
      locateH(area, ss);
      return true;
    }
    // If we couldn't parse, just fall through (no native FIND to call).
    return false;
  }

  // 2) LOCATE fallback -> (ensure index) -> FIND (only if native LOCATE missing)
  // --- Convenience: if native LOCATE exists, accept "LOCATE <field> [=] <value>"
  // and rewrite to "LOCATE FOR <field> = <value>" before invoking the handler.
  if (key == "LOCATE" && haveMain) {
    // If the arg already starts with "FOR", leave it to the native handler.
    if (!(rem.size() >= 3 && upcopy(rem.substr(0,3)) == "FOR")) {
      auto toks = split_tokens(rem);
      if (toks.size() >= 2) {
        std::string field = toks[0];
        std::string value;
        if (toks.size() >= 3 && (toks[1] == "=" || toks[1] == "==")) {
          for (size_t i = 2; i < toks.size(); ++i) { if (i > 2) value.push_back(' '); value += toks[i]; }
        } else {
          for (size_t i = 1; i < toks.size(); ++i) { if (i > 1) value.push_back(' '); value += toks[i]; }
        }
        bool quoted = (!value.empty() &&
                       ((value.front()=='"' && value.back()=='"') ||
                        (value.front()=='\'' && value.back()=='\'')));
        if (!quoted && !is_numeric_like(value)) value = "\"" + value + "\"";

        std::string locateArgs = "FOR " + field + " = " + value;
        std::istringstream ss(locateArgs);
        mainH(area, ss);
        return true;
      }
      // If we couldn't parse a shorthand, fall through to native LOCATE usage error.
    }
  }

  // ---------------------- Execute native handler ----------------------
  std::istringstream callArgs(rem);
  mainH(area, callArgs);
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

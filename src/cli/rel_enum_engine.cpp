// src/cli/rel_enum_engine.cpp
#include "rel_enum_engine.hpp"

#include "set_relations.hpp"
#include "xbase.hpp"
#include "workareas.hpp"
#include "xbase_field_getters.hpp"
#include "textio.hpp"
#include "command_registry.hpp"

#include <cctype>
#include <cstddef>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <streambuf>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

extern "C" xbase::XBaseEngine* shell_engine();

namespace
{
    static std::string trim(std::string s) { return textio::trim(std::move(s)); }
    static std::string up(std::string s)   { return textio::up(std::move(s)); }

    static int slot_of_area(xbase::DbArea* area)
    {
        if (!area) return -1;
        const std::size_t n = workareas::count();
        for (std::size_t i = 0; i < n; ++i) {
            if (workareas::at(i) == area) return static_cast<int>(i);
        }
        return -1;
    }

    class ScopedAreaSelect {
    public:
        explicit ScopedAreaSelect(xbase::DbArea* area) noexcept {
            eng_ = shell_engine();
            if (!eng_ || !area) return;

            const int slot = slot_of_area(area);
            if (slot < 0) return;

            try {
                prev_ = eng_->currentArea();
                if (prev_ != slot) {
                    eng_->selectArea(slot);
                    active_ = true;
                }
            } catch (...) { active_ = false; }
        }

        ~ScopedAreaSelect() noexcept {
            if (!active_ || !eng_) return;
            try { eng_->selectArea(prev_); } catch (...) {}
        }

        ScopedAreaSelect(const ScopedAreaSelect&) = delete;
        ScopedAreaSelect& operator=(const ScopedAreaSelect&) = delete;

    private:
        xbase::XBaseEngine* eng_{nullptr};
        int prev_{-1};
        bool active_{false};
    };

    class ScopedEngineArea {
    public:
        ScopedEngineArea() noexcept {
            eng_ = shell_engine();
            if (!eng_) return;
            try {
                prev_ = eng_->currentArea();
                active_ = true;
            } catch (...) { active_ = false; }
        }

        ~ScopedEngineArea() noexcept {
            if (!active_ || !eng_) return;
            try { eng_->selectArea(prev_); } catch (...) {}
        }

        ScopedEngineArea(const ScopedEngineArea&) = delete;
        ScopedEngineArea& operator=(const ScopedEngineArea&) = delete;

    private:
        xbase::XBaseEngine* eng_{nullptr};
        int prev_{-1};
        bool active_{false};
    };

    class ScopedStdoutCapture {
    public:
        ScopedStdoutCapture() : old_(std::cout.rdbuf(buf_.rdbuf())) {}
        ~ScopedStdoutCapture() { std::cout.rdbuf(old_); }

        std::string str() const { return buf_.str(); }

        ScopedStdoutCapture(const ScopedStdoutCapture&) = delete;
        ScopedStdoutCapture& operator=(const ScopedStdoutCapture&) = delete;

    private:
        std::ostringstream buf_;
        std::streambuf* old_{nullptr};
    };

    static xbase::DbArea* find_open_area_by_name_ci(const std::string& logical_or_name)
    {
        const std::string target = up(trim(logical_or_name));
        if (target.empty()) return nullptr;

        const std::size_t n = workareas::count();
        for (std::size_t i = 0; i < n; ++i) {
            xbase::DbArea* a = workareas::at(i);
            if (!a) continue;

            bool open = false;
            try { open = a->isOpen(); } catch (...) { open = false; }
            if (!open) continue;

            try {
                const std::string ln = a->logicalName();
                if (!ln.empty() && up(ln) == target) return a;
                const std::string nm = a->name();
                if (!nm.empty() && up(nm) == target) return a;
            } catch (...) {}
        }
        return nullptr;
    }

    static std::vector<std::string> split_tuple_expr_csv(const std::string& s)
    {
        std::vector<std::string> out;
        std::string cur;
        int paren_depth = 0;
        bool in_quote = false;

        for (std::size_t i = 0; i < s.size(); ++i) {
            const char c = s[i];

            if (c == '"' && (i == 0 || s[i - 1] != '\\')) {
                in_quote = !in_quote;
                cur.push_back(c);
                continue;
            }
            if (!in_quote) {
                if (c == '(') { ++paren_depth; cur.push_back(c); continue; }
                if (c == ')' && paren_depth > 0) { --paren_depth; cur.push_back(c); continue; }
                if (c == ',' && paren_depth == 0) {
                    std::string t = trim(cur);
                    if (!t.empty()) out.push_back(std::move(t));
                    cur.clear();
                    continue;
                }
            }
            cur.push_back(c);
        }

        std::string t = trim(cur);
        if (!t.empty()) out.push_back(std::move(t));
        return out;
    }

    static bool parse_field_ref(const std::string& term,
                                std::string& area_name_out,
                                std::string& field_name_out)
    {
        std::string t = trim(term);
        if (t.empty()) return false;

        for (char c : t) {
            if (c == '(' || c == ')' || c == '"' || c == '\'' || c == '+' || c == '-' ||
                c == '*' || c == '/' || c == '%' || c == '<' || c == '>' || c == '=' ||
                c == '&' || c == '|' || c == '!') {
                return false;
            }
        }

        const std::size_t dot = t.find('.');
        if (dot == std::string::npos) {
            area_name_out.clear();
            field_name_out = trim(t);
            return !field_name_out.empty();
        }

        area_name_out = trim(t.substr(0, dot));
        field_name_out = trim(t.substr(dot + 1));
        return !area_name_out.empty() && !field_name_out.empty();
    }

    static bool build_distinct_key_from_exprs(const std::vector<std::string>& tuple_exprs,
                                              xbase::DbArea* home_area,
                                              std::string& key_out)
    {
        key_out.clear();
        if (tuple_exprs.empty()) return false;

        bool all_ok = true;
        std::string key;

        for (const auto& term : tuple_exprs) {
            std::string area_name, field_name;
            if (!parse_field_ref(term, area_name, field_name)) {
                all_ok = false;
                break;
            }

            xbase::DbArea* db = nullptr;
            if (!area_name.empty()) db = find_open_area_by_name_ci(area_name);
            if (!db) db = home_area;
            if (!db) { all_ok = false; break; }

            std::string val;
            try {
                ScopedAreaSelect focus(db);
                db->readCurrent();
                val = xfg::getFieldAsString(*db, field_name);
            } catch (...) {
                all_ok = false;
                break;
            }

            if (!key.empty()) key.push_back('\x1f');
            key += trim(val);
        }

        if (!all_ok) return false;
        key_out = std::move(key);
        return true;
    }

    static std::string join_tuple_exprs_csv(const std::vector<std::string>& exprs)
    {
        std::string out;
        for (std::size_t i = 0; i < exprs.size(); ++i) {
            if (i) out += ", ";
            out += exprs[i];
        }
        return out;
    }

    static std::vector<std::string> split_pipe_row(const std::string& line)
    {
        std::vector<std::string> out;
        std::string cur;
        for (char c : line) {
            if (c == '|') {
                out.push_back(trim(cur));
                cur.clear();
            } else {
                cur.push_back(c);
            }
        }
        out.push_back(trim(cur));
        return out;
    }

    static void append_captured_rows(const std::string& captured,
                                     rel_enum_engine::Result& out)
    {
        std::istringstream iss(captured);
        std::string line;
        while (std::getline(iss, line)) {
            line = trim(line);
            if (line.empty()) continue;
            if (up(line) == "OK") continue;

            rel_enum_engine::Row row{};
            row.cells = split_pipe_row(line);
            out.rows.push_back(std::move(row));
        }
    }

    static void fill_counts_for_path(const std::vector<std::string>& path_aliases,
                                     std::vector<rel_enum_engine::Count>& out_counts)
    {
        out_counts.clear();

        // Uses the already-available tree listing to populate per-hop match counts.
        // Long-term note:
        //   cycle/visited safety belongs inside relations_api traversal/listing code,
        //   not here in the browser/enum adapters.
        const auto rows = relations_api::list_tree_for_current_parent(/*recursive=*/true, /*max_depth=*/24);

        std::map<std::string, int> counts_by_alias;

        for (const auto& r : rows) {
            const std::string line = r.line;

            const std::size_t arrow = line.find("->");
            if (arrow == std::string::npos) continue;

            std::size_t p = arrow + 2;
            while (p < line.size() && std::isspace(static_cast<unsigned char>(line[p]))) ++p;

            std::size_t q = p;
            while (q < line.size() && !std::isspace(static_cast<unsigned char>(line[q]))) ++q;

            const std::string alias = trim(line.substr(p, q - p));
            if (alias.empty()) continue;

            const std::string needle = "(matches:";
            const std::size_t m = line.find(needle);
            if (m == std::string::npos) continue;

            std::size_t n = m + needle.size();
            while (n < line.size() && std::isspace(static_cast<unsigned char>(line[n]))) ++n;

            std::size_t e = n;
            while (e < line.size() && std::isdigit(static_cast<unsigned char>(line[e]))) ++e;

            int count = 0;
            try {
                count = std::stoi(line.substr(n, e - n));
            } catch (...) {
                count = 0;
            }

            counts_by_alias[up(alias)] = count;
        }

        out_counts.reserve(path_aliases.size());
        for (const auto& a : path_aliases) {
            rel_enum_engine::Count c{};
            c.alias = a;
            const auto it = counts_by_alias.find(up(a));
            c.count = (it == counts_by_alias.end()) ? 0 : it->second;
            out_counts.push_back(std::move(c));
        }
    }
} // namespace

namespace rel_enum_engine
{
    bool run(xbase::DbArea& area, const Request& req, Result& out)
    {
        out = Result{};

        if (req.tuple_exprs.empty()) {
            out.status = "ERROR";
            out.warnings.push_back("REL ENUM engine: missing tuple expressions.");
            return false;
        }

        xbase::DbArea* home = &area;

        const std::string tuple_csv = join_tuple_exprs_csv(req.tuple_exprs);
        std::unordered_set<std::string> seen;
        std::size_t emitted = 0;

        {
            ScopedAreaSelect focus(&area);

            const bool ok = relations_api::enum_emit_for_current_parent(
                req.path_aliases,
                req.limit,
                [&]() {
                    ScopedEngineArea keep_area;

                    if (!req.distinct) {
                        ScopedStdoutCapture cap;
                        std::istringstream t(tuple_csv);
                        dli::registry().run(area, "TUPLE", t);
                        append_captured_rows(cap.str(), out);
                        return;
                    }

                    std::string key;
                    const bool have_key = build_distinct_key_from_exprs(req.tuple_exprs, home, key);
                    if (!have_key) {
                        // Same fallback behavior as existing command:
                        // if DISTINCT key cannot be built, emit anyway.
                        ScopedStdoutCapture cap;
                        std::istringstream t(tuple_csv);
                        dli::registry().run(area, "TUPLE", t);
                        append_captured_rows(cap.str(), out);
                        return;
                    }

                    if (seen.insert(key).second) {
                        ScopedStdoutCapture cap;
                        std::istringstream t(tuple_csv);
                        dli::registry().run(area, "TUPLE", t);
                        append_captured_rows(cap.str(), out);
                    }
                },
                &emitted
            );

            if (!ok) {
                out.status = "ERROR";
                out.warnings.push_back("REL ENUM engine: enum_emit_for_current_parent failed.");
                return false;
            }

            fill_counts_for_path(req.path_aliases, out.counts);
        }

        if (out.status.empty())
            out.status = out.warnings.empty() ? "OK" : "WARN";

        return true;
    }
}
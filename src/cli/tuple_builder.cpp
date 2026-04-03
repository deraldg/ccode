// src/cli/tuple_builder.cpp
#include "tuple_builder.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "cli/table_state.hpp"
#include "cli/memo_display.hpp"
#include "set_relations.hpp"
#include "workareas.hpp"
#include "xbase.hpp"
#include "xbase_field_getters.hpp"

namespace {

std::string trim(std::string s) {
    auto issp = [](unsigned char c) { return std::isspace(c) != 0; };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&](unsigned char c) { return !issp(c); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [&](unsigned char c) { return !issp(c); }).base(), s.end());
    return s;
}

std::string up(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

void strip_inline_comment(std::string& s) {
    auto cut_at = [&](std::size_t pos) {
        if (pos != std::string::npos) s.erase(pos);
    };

    std::size_t p = s.find("&&");
    if (p != std::string::npos) { cut_at(p); return; }

    p = s.find("//");
    if (p != std::string::npos) { cut_at(p); return; }

    p = s.find(" (");
    if (p != std::string::npos) { cut_at(p); return; }

    p = s.find(" ;");
    if (p != std::string::npos) { cut_at(p); return; }
}

bool area_is_open(const xbase::DbArea* a) {
    if (!a) return false;
    try { return a->isOpen(); } catch (...) { return false; }
}

const xbase::DbArea* resolve_area(std::size_t slot) {
    if (slot >= workareas::count()) return nullptr;
    return workareas::at(slot);
}

int current_slot() {
    return static_cast<int>(workareas::current_slot());
}

std::string basename_upper(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    const auto slash = path.find_last_of('/');
    if (slash != std::string::npos) path.erase(0, slash + 1);
    const auto dot = path.find_last_of('.');
    if (dot != std::string::npos) path.erase(dot);
    return up(path);
}

std::string area_display_name_upper(int slot) {
    const xbase::DbArea* A = resolve_area(static_cast<std::size_t>(slot));
    if (!area_is_open(A)) return "";
    try { return basename_upper(A->name()); } catch (...) { return ""; }
}

// If TABLE buffering is enabled for an area, return the highest-priority buffered value for (recno, field1).
bool get_buffer_override(int area0, int recno, int field1, std::string& out_val) {
    if (!dottalk::table::in_range(area0)) return false;
    if (!dottalk::table::is_enabled(area0)) return false;

    const auto& tb = dottalk::table::get_tb_const(area0);
    if (tb.empty()) return false;

    const auto range = tb.changes.equal_range(recno);
    if (range.first == range.second) return false;

    bool found = false;
    int best_prio = -2147483647;
    std::string best;

    for (auto it = range.first; it != range.second; ++it) {
        const auto& ce = it->second;
        const auto nv = ce.new_values.find(field1);
        if (nv == ce.new_values.end()) continue;

        if (!found || ce.priority >= best_prio) {
            found = true;
            best_prio = ce.priority;
            best = nv->second;
        }
    }

    if (!found) return false;
    out_val = best;
    return true;
}

struct NameResolve {
    int slot = -1;
    bool ambiguous = false;
    std::vector<int> matches;
};

NameResolve resolve_slot_by_area_name_all(const std::string& areaNameUpper) {
    NameResolve r;
    const std::size_t n = workareas::count();

    for (std::size_t i = 0; i < n; ++i) {
        const xbase::DbArea* A = resolve_area(i);
        if (!area_is_open(A)) continue;

        bool matched = false;

        // optional alias source
        try {
            const std::string alias = up(std::string(workareas::name(i)));
            if (!alias.empty() && alias == areaNameUpper) matched = true;
        } catch (...) {}

        if (!matched) {
            try {
                const std::string base = basename_upper(A->name());
                if (!base.empty() && base == areaNameUpper) matched = true;
            } catch (...) {}
        }

        if (matched) r.matches.push_back(static_cast<int>(i));
    }

    if (r.matches.size() == 1) {
        r.slot = r.matches[0];
    } else if (!r.matches.empty()) {
        r.ambiguous = true;
    }

    return r;
}

struct FieldRef {
    int area_slot = -1;     // -1 => current slot
    std::string field;      // FIELD or "*"
};

struct ParseResult {
    bool ok = true;
    std::string error;
    std::vector<FieldRef> refs;
};

// Tokens: "*", "FIELD", "#n.FIELD|*", "AREA.FIELD|*"
ParseResult parse_tokens(const std::vector<std::string>& toks) {
    ParseResult pr;

    for (auto tok : toks) {
        strip_inline_comment(tok);
        tok = trim(tok);
        if (tok.empty()) continue;

        if (!tok.empty() && tok[0] == '#') {
            std::size_t i = 1;
            while (i < tok.size() && std::isdigit(static_cast<unsigned char>(tok[i])) != 0) ++i;

            int slot = -1;
            try { slot = std::stoi(tok.substr(1, i - 1)); } catch (...) { slot = -1; }

            if (i < tok.size() && tok[i] == '.') ++i;
            std::string rest = trim((i <= tok.size()) ? tok.substr(i) : std::string{});
            if (rest.empty()) rest = "*";

            pr.refs.push_back(FieldRef{slot, rest});
            continue;
        }

        const std::size_t dot = tok.find('.');
        if (dot != std::string::npos) {
            const std::string area = trim(tok.substr(0, dot));
            std::string field = trim(tok.substr(dot + 1));
            if (field.empty()) field = "*";

            const NameResolve nr = resolve_slot_by_area_name_all(up(area));
            if (nr.ambiguous) {
                std::ostringstream oss;
                oss << "ERROR: TUPLE area name '" << area << "' is ambiguous; matching slots: ";
                for (std::size_t k = 0; k < nr.matches.size(); ++k) {
                    if (k) oss << ", ";
                    oss << nr.matches[k];
                }
                oss << ". Use #<slot>.";
                pr.ok = false;
                pr.error = oss.str();
                return pr;
            }

            if (nr.slot >= 0) {
                pr.refs.push_back(FieldRef{nr.slot, field});
                continue;
            }

            // Unknown name -> keep literal token; will resolve to empty value later.
            pr.refs.push_back(FieldRef{-1, tok});
            continue;
        }

        pr.refs.push_back(FieldRef{-1, tok}); // FIELD or "*"
    }

    return pr;
}

std::vector<std::pair<int, std::string>> expand(const FieldRef& r) {
    std::vector<std::pair<int, std::string>> out;

    const int slot = (r.area_slot >= 0) ? r.area_slot : current_slot();
    const xbase::DbArea* A = resolve_area(static_cast<std::size_t>(slot));

    if (r.field == "*") {
        if (!area_is_open(A)) return out;
        try {
            for (const auto& fd : A->fields()) out.emplace_back(slot, fd.name);
        } catch (...) {}
        return out;
    }

    out.emplace_back(slot, r.field);
    return out;
}

int safe_recno(const xbase::DbArea* A) {
    if (!A) return 0;
    try { return static_cast<int>(A->recno()); } catch (...) { return 0; }
}

int resolve_field1(const xbase::DbArea* ar, const std::string& canonicalField) {
    if (!area_is_open(ar)) return 0;

    try {
        const auto& fs = ar->fields();
        const std::string want = up(canonicalField);
        for (std::size_t fi = 0; fi < fs.size(); ++fi) {
            if (up(fs[fi].name) == want) return static_cast<int>(fi) + 1; // 1-based field #
        }
    } catch (...) {}

    return 0;
}

} // namespace

namespace dottalk {

TupleBuildResult build_tuple_default(const TupleBuildOptions& opt) {
    return build_tuple_from_spec("*", opt);
}

TupleBuildResult build_tuple_from_spec(const std::string& spec_in, const TupleBuildOptions& opt) {
    TupleBuildResult res;

    if (opt.refresh_relations) {
        relations_api::refresh_for_current_parent();
    }

    std::string spec = trim(spec_in);
    if (spec.empty()) spec = "*";

    // tokenize by comma
    std::vector<std::string> toks;
    {
        std::stringstream ss(spec);
        std::string tok;
        while (std::getline(ss, tok, ',')) toks.push_back(tok);
    }

    const auto pr = parse_tokens(toks);
    if (!pr.ok) {
        res.ok = false;
        res.error = pr.error;
        return res;
    }

    std::vector<std::pair<int, std::string>> items;
    for (const auto& r : pr.refs) {
        auto v = expand(r);
        items.insert(items.end(), v.begin(), v.end());
    }

    if (items.empty()) {
        res.ok = true;
        res.row = TupleRow{};
        return res;
    }

    TupleRow row;
    row.columns.reserve(items.size());
    row.values.reserve(items.size());

    std::unordered_set<int> touched_slots;

    for (const auto& item : items) {
        const int slot = item.first;
        const std::string fieldName = item.second;

        const xbase::DbArea* ar = resolve_area(static_cast<std::size_t>(slot));
        std::string canonicalField = fieldName;

        // Resolve canonical case by schema match
        if (area_is_open(ar)) {
            try {
                const auto& fs = ar->fields();
                const std::string want = up(fieldName);
                for (const auto& fd : fs) {
                    if (up(fd.name) == want) {
                        canonicalField = fd.name;
                        break;
                    }
                }
            } catch (...) {}
        }

        // Column label
        std::string colName = canonicalField;
        if (opt.header_area_prefix) {
            const std::string areaName = area_display_name_upper(slot);
            if (!areaName.empty()) colName = areaName + "." + canonicalField;
        }

        // Base value from DBF
        std::string val;
        const bool have_area = area_is_open(ar);
        int field1 = 0;

        if (have_area) {
            try {
                bool matched = false;
                const std::string want = up(canonicalField);
                const auto& fs = ar->fields();
                for (std::size_t fi = 0; fi < fs.size(); ++fi) {
                    const auto& fd = fs[fi];
                    if (up(fd.name) == want) {
                        field1 = static_cast<int>(fi) + 1; // 1-based
                        val = xfg::getFieldAsString(*const_cast<xbase::DbArea*>(ar), fd.name);
                        matched = true;
                        break;
                    }
                }

                if (!matched) {
                    if (opt.strict_fields) {
                        res.ok = false;
                        std::ostringstream oss;
                        oss << "ERROR: field '" << fieldName << "' not found in area slot " << slot << ".";
                        res.error = oss.str();
                        return res;
                    }

                    field1 = resolve_field1(ar, canonicalField);
                    val = xfg::getFieldAsString(*const_cast<xbase::DbArea*>(ar), canonicalField);
                }
            } catch (...) {
                val.clear();
                field1 = 0;
            }
        } else if (opt.strict_fields) {
            res.ok = false;
            std::ostringstream oss;
            oss << "ERROR: area slot " << slot << " is not open.";
            res.error = oss.str();
            return res;
        }

        // Overlay TABLE-buffered edits (preview) for this area+recno+field.
        if (have_area) {
            const int area0 = slot;
            const int recno = safe_recno(ar);

            std::string ov;
            if (field1 > 0 && get_buffer_override(area0, recno, field1, ov)) {
                val = ov;
            }
        }

        // Resolve x64 memo object-id strings to display text.
        if (have_area && field1 > 0) {
            try {
                val = cli_memo::resolve_display_value(*const_cast<xbase::DbArea*>(ar), field1, val);
            } catch (...) {
                // Leave raw value unchanged on lookup failure.
            }
        }

        row.columns.push_back(TupleColumn{colName, slot, canonicalField});
        row.values.push_back(val);

        if (have_area) touched_slots.insert(slot);
    }

    // Fragments: DBF-level provenance
    for (int slot : touched_slots) {
        const xbase::DbArea* ar = resolve_area(static_cast<std::size_t>(slot));
        if (!area_is_open(ar)) continue;

        TupleFragment f;
        f.area_slot = slot;
        f.recno = safe_recno(ar);
        f.kind = TupleSourceKind::DBF;

        try {
            f.note = "DBF:" + basename_upper(ar->name());
        } catch (...) {
            f.note = "DBF";
        }

        row.fragments.push_back(std::move(f));
    }

    res.ok = true;
    res.row = std::move(row);
    return res;
}

} // namespace dottalk
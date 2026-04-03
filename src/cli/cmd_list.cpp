// src/cli/cmd_list.cpp
#include "xbase.hpp"
#include "xindex/index_manager.hpp"
#include "textio.hpp"
#include "filters/filter_registry.hpp"
#include "cli/expr/api.hpp"
#include "cli/expr/ast.hpp"
#include "cli/expr/glue_xbase.hpp"
#include "cli/expr/parse_utils.hpp"
#include "cli/expr/line_parse_utils.hpp"
#include "value_normalize.hpp"
#include "cli/order_state.hpp"
#include "cli/order_iterator.hpp"
#include "cli/cmd_nav_move.hpp"
#include "cli/expr/normalize_where.hpp"
#include "workareas.hpp"
#include "workspace/workarea_utils.hpp"


#include <iostream>
#include <iomanip>
#include <string>
#include <cctype>
#include <algorithm>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace {

struct CursorRestore {
    xbase::DbArea* a{nullptr};
    int32_t saved{0};
    bool active{false};

    explicit CursorRestore(xbase::DbArea& area) : a(&area) {
        saved = area.recno();
        active = (saved >= 1 && saved <= area.recCount());
    }

    void cancel() noexcept { active = false; }

    ~CursorRestore() {
        if (!active || !a) return;
        try {
            a->gotoRec(saved);
            a->readCurrent();
        } catch (...) {
            // best-effort
        }
    }
};

struct CursorLineInfo {
    std::size_t area_slot0{0};
    std::size_t area_count{0};
    int32_t physical_recno{0};
    std::int64_t logical_row{0};
};

enum class DelFilter {
    Any,
    OnlyDeleted,
    OnlyAlive
};

enum class StartPos {
    Here,
    Top,
    Bottom
};

struct Options {
    bool all{false};
    int  limit{20};
    StartPos start{StartPos::Here};

    DelFilter del{DelFilter::Any};

    bool haveCompiledFor{false};
    std::string forExpr;
};

static inline std::string upper_copy(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

static Options parse_opts(std::istringstream& iss) {
    Options o{};

    std::string tok;
    auto save = iss.tellg();
    if (iss >> tok) {
        if (textio::ieq(tok, "TOP")) {
            o.start = StartPos::Top;
        } else if (textio::ieq(tok, "BOTTOM")) {
            o.start = StartPos::Bottom;
        } else {
            iss.clear();
            iss.seekg(save);
        }
    }

    save = iss.tellg();
    if (iss >> tok) {
        if (textio::ieq(tok, "ALL")) {
            o.all = true;
        } else if (textio::ieq(tok, "DELETED")) {
            o.del = DelFilter::OnlyDeleted;
        } else if (is_uint(tok)) {
            o.limit = std::max(0, std::stoi(tok));
        } else {
            iss.clear();
            iss.seekg(save);
        }
    }

    save = iss.tellg();
    std::string w;
    if (iss >> w && textio::ieq(w, "FOR")) {
        std::string a;
        if (iss >> a) {
            const std::string ua = upper_copy(a);

            // Special engine-state shortcuts stay outside AST.
            if (ua == "DELETED") {
                o.del = DelFilter::OnlyDeleted;
                return o;
            }
            if (ua == "!DELETED" || ua == "~DELETED") {
                o.del = DelFilter::OnlyAlive;
                return o;
            }

            // Everything else goes through the AST expression path.
            std::string rest_of_line;
            std::getline(iss, rest_of_line);
            std::string expr = a;
            const std::string tail = textio::trim(rest_of_line);
            if (!tail.empty()) expr += " " + tail;

            const std::string cleaned = textio::trim(strip_line_comments(expr));
            if (!cleaned.empty()) {
                o.haveCompiledFor = true;
                o.forExpr = cleaned;
            }
            return o;
        } else {
            iss.clear();
            iss.seekg(save);
        }
    } else {
        iss.clear();
        iss.seekg(save);
    }

    return o;
}

static int recno_width(const xbase::DbArea& a) {
    int n = std::max(1, a.recCount());
    int w = 0;
    while (n) {
        n /= 10;
        ++w;
    }
    return std::max(3, w);
}

static void header(const xbase::DbArea& a, int recw) {
    const auto& Fs = a.fields();
    std::cout << "  " << std::setw(recw) << "" << " ";
    for (const auto& f : Fs) {
        std::cout << std::left << std::setw(static_cast<int>(f.length)) << f.name << " ";
    }
    std::cout << std::right << "\n";
}

static void row(const xbase::DbArea& a, int recw) {
    const auto& Fs = a.fields();
    std::cout << ' ' << (a.isDeleted() ? '*' : ' ') << ' '
              << std::setw(recw) << a.recno() << " ";

    for (int i = 1; i <= static_cast<int>(Fs.size()); ++i) {
        std::string s = a.get(i);
        const int w = static_cast<int>(Fs[static_cast<std::size_t>(i - 1)].length);
        if (static_cast<int>(s.size()) > w) {
            s.resize(static_cast<std::size_t>(w));
        }
        std::cout << std::left << std::setw(w) << s << " ";
    }
    std::cout << std::right << "\n";
}

static inline bool pass_deleted_filter(const xbase::DbArea& a, const Options& o) {
    const bool isDel = a.isDeleted();
    switch (o.del) {
        case DelFilter::OnlyDeleted: return isDel;
        case DelFilter::OnlyAlive:   return !isDel;
        case DelFilter::Any:
        default:                     return o.all ? true : !isDel;
    }
}

static inline bool pass_all_filters(xbase::DbArea& a,
                                    const Options& opt,
                                    const std::shared_ptr<dottalk::expr::Expr>& prog)
{
    if (!pass_deleted_filter(a, opt)) return false;

    if (!filter::visible(&a, prog)) return false;

    return true;
}

static CursorLineInfo make_cursor_line_info(int32_t physical_recno, std::int64_t logical_row) {
    CursorLineInfo info{};
    info.area_slot0      = workareas::current_slot();
    info.area_count      = workareas::count();
    info.physical_recno  = physical_recno;
    info.logical_row     = logical_row;
    return info;
}

static void print_cursor_line(const CursorLineInfo& info) {
    std::cout << "Cursor: Area " << info.area_slot0
              << " of " << workareas::occupied_desc()
              << "  Physical Recno " << info.physical_recno
              << ", Logical Row " << info.logical_row << "\n";
}

static std::int64_t compute_physical_logical_row(xbase::DbArea& a,
                                                 int32_t physical_recno,
                                                 const Options& opt,
                                                 const std::shared_ptr<dottalk::expr::Expr>& prog)
{
    if (physical_recno < 1 || physical_recno > a.recCount()) return 0;

    std::int64_t logical_row = 0;
    for (int32_t rn = 1; rn <= physical_recno; ++rn) {
        if (!a.gotoRec(rn) || !a.readCurrent()) continue;
        if (!pass_all_filters(a, opt, prog)) continue;
        ++logical_row;
    }
    return logical_row;
}

static std::int64_t compute_ordered_logical_row(xbase::DbArea& a,
                                                const std::vector<uint64_t>& recnos_asc,
                                                const cli::OrderIterSpec& spec,
                                                int32_t physical_recno,
                                                const Options& opt,
                                                const std::shared_ptr<dottalk::expr::Expr>& prog)
{
    if (physical_recno < 1 || physical_recno > a.recCount()) return 0;

    std::int64_t logical_row = 0;

    if (spec.ascending) {
        for (uint64_t rn64 : recnos_asc) {
            const int32_t rn = static_cast<int32_t>(rn64);
            if (!a.gotoRec(rn) || !a.readCurrent()) continue;
            if (!pass_all_filters(a, opt, prog)) continue;
            ++logical_row;
            if (rn == physical_recno) return logical_row;
        }
    } else {
        for (auto it = recnos_asc.rbegin(); it != recnos_asc.rend(); ++it) {
            const int32_t rn = static_cast<int32_t>(*it);
            if (!a.gotoRec(rn) || !a.readCurrent()) continue;
            if (!pass_all_filters(a, opt, prog)) continue;
            ++logical_row;
            if (rn == physical_recno) return logical_row;
        }
    }

    return 0;
}

static const char* backend_name(const cli::OrderIterSpec& spec) {
    switch (spec.backend) {
        case cli::OrderBackend::Natural: return "natural";
        case cli::OrderBackend::Inx:     return "inx";
        case cli::OrderBackend::Cnx:     return "cnx";
        case cli::OrderBackend::Cdx:
            return (spec.cdx_mode == cli::CdxExecMode::Lmdb) ? "cdx(lmdb)" : "cdx(fallback)";
        case cli::OrderBackend::Isx:     return "isx";
        case cli::OrderBackend::Csx:     return "csx";
        default:                         return "ordered";
    }
}

static void print_order_banner(const cli::OrderIterSpec& spec) {
    if (spec.backend == cli::OrderBackend::Natural) return;

    std::cout << "; ORDER: file '" << spec.container_path << "'";
    if (!spec.tag.empty()) {
        std::cout << "  TAG '" << upper_copy(spec.tag) << "'";
    }
    if (spec.backend == cli::OrderBackend::Cdx) {
        std::cout << "  MODE " << ((spec.cdx_mode == cli::CdxExecMode::Lmdb) ? "LMDB" : "FALLBACK");
    }
    std::cout << "  " << (spec.ascending ? "ASC" : "DESC") << "\n";
}

static void print_summary(int printed,
                          const Options& opt,
                          const cli::OrderIterSpec& spec,
                          const CursorLineInfo& cursor)
{
    if (spec.backend == cli::OrderBackend::Natural) {
        if (!opt.all) {
            std::cout << printed << " record(s) listed (limit " << opt.limit
                      << "). Use LIST ALL to show more.\n";
        } else {
            std::cout << printed << " record(s) listed.\n";
        }
        print_cursor_line(cursor);
        return;
    }

    std::cout << printed << " " << backend_name(spec) << " indexed record(s)";
    if (!spec.tag.empty()) {
        std::cout << " (tag=" << upper_copy(spec.tag) << ")";
    }
    if (!opt.all) {
        std::cout << " listed (limit " << opt.limit << "). Use LIST ALL to show more.\n";
    } else {
        std::cout << " listed.\n";
    }
    print_cursor_line(cursor);
}

static bool list_from_recnos(xbase::DbArea& a,
                             const std::vector<uint64_t>& recnos_asc,
                             const cli::OrderIterSpec& spec,
                             const Options& opt,
                             const std::shared_ptr<dottalk::expr::Expr>& prog,
                             int recw,
                             int32_t start_rn,
                             const CursorLineInfo& cursor)
{
    int printed = 0;
    bool started = (start_rn == 0);

    if (spec.ascending) {
        for (uint64_t rn : recnos_asc) {
            if (!started) {
                if (static_cast<int32_t>(rn) != start_rn) continue;
                started = true;
            }
            if (!a.gotoRec(static_cast<int32_t>(rn)) || !a.readCurrent()) continue;
            if (!pass_all_filters(a, opt, prog)) continue;
            row(a, recw);
            ++printed;
            if (!opt.all && opt.limit > 0 && printed >= opt.limit) break;
        }
    } else {
        for (auto it = recnos_asc.rbegin(); it != recnos_asc.rend(); ++it) {
            const uint64_t rn = *it;
            if (!started) {
                if (static_cast<int32_t>(rn) != start_rn) continue;
                started = true;
            }
            if (!a.gotoRec(static_cast<int32_t>(rn)) || !a.readCurrent()) continue;
            if (!pass_all_filters(a, opt, prog)) continue;
            row(a, recw);
            ++printed;
            if (!opt.all && opt.limit > 0 && printed >= opt.limit) break;
        }
    }

    print_summary(printed, opt, spec, cursor);
    return true;
}

} // namespace

void cmd_LIST(xbase::DbArea& a, std::istringstream& iss) {
    CursorRestore _restore_(a);

    if (!a.isOpen()) {
        std::cout << "No table open.\n";
        return;
    }

    Options opt = parse_opts(iss);

    std::shared_ptr<dottalk::expr::Expr> _prog;
    if (opt.haveCompiledFor) {
        std::string norm = normalize_unquoted_rhs_literals(a, opt.forExpr);
        auto cr = dottalk::expr::compile_where(norm);
//      auto cr = dottalk::expr::compile_where(opt.forExpr);
        if (cr) {
            _prog = std::shared_ptr<dottalk::expr::Expr>(std::move(cr.program));
        } else {
            std::cout << "; LIST FOR error: " << cr.error << " - ignoring FOR.\n";
            opt.haveCompiledFor = false;
        }
    }

    const int32_t total = a.recCount();
    if (total <= 0) {
        std::cout << "(empty)\n";
        return;
    }

    bool nav_ok = true;
    switch (opt.start) {
        case StartPos::Top:
            nav_ok = cli::nav::go_endpoint(a, cli::nav::Endpoint::Top, "LIST");
            break;
        case StartPos::Bottom:
            nav_ok = cli::nav::go_endpoint(a, cli::nav::Endpoint::Bottom, "LIST");
            break;
        case StartPos::Here:
        default:
            if (opt.all) nav_ok = a.top();
            else if (a.recno() <= 0) nav_ok = a.top();
            break;
    }
    if (!nav_ok) return;

    const int recw = recno_width(a);
    header(a, recw);

    if (orderstate::hasOrder(a)) {
        cli::OrderIterSpec spec{};
        std::vector<uint64_t> recnos_asc;
        std::string err;

        const bool ok = cli::order_collect_recnos_asc(a, recnos_asc, &spec, &err);
        const int32_t start_rn =
            (!opt.all && a.recno() >= 1 && a.recno() <= a.recCount()) ? a.recno() : 0;

        if (ok && !recnos_asc.empty()) {
            const std::int64_t logical_row =
                compute_ordered_logical_row(a, recnos_asc, spec, start_rn, opt, _prog);
            const CursorLineInfo cursor = make_cursor_line_info(start_rn, logical_row);
            print_order_banner(spec);
            list_from_recnos(a, recnos_asc, spec, opt, _prog, recw, start_rn, cursor);
            return;
        }

        if (!err.empty()) {
            std::cout << "; LIST order note: " << err << " - falling back to physical order\n";
        } else {
            std::cout << "; LIST order note: ordered backend unavailable - falling back to physical order\n";
        }
    }

    int printed = 0;
    const int32_t start = opt.all ? 1 : a.recno();

    for (int32_t rn = start; rn <= total; ++rn) {
        if (!a.gotoRec(rn) || !a.readCurrent()) continue;
        if (!pass_all_filters(a, opt, _prog)) continue;
        row(a, recw);
        ++printed;
        if (!opt.all && opt.limit > 0 && printed >= opt.limit) break;
    }

    cli::OrderIterSpec natural_spec{};
    natural_spec.backend = cli::OrderBackend::Natural;
    natural_spec.cdx_mode = cli::CdxExecMode::Fallback;
    natural_spec.ascending = true;

    const int32_t current_physical =
        (a.recno() >= 1 && a.recno() <= a.recCount()) ? a.recno() : 0;

    const std::int64_t logical_row =
        compute_physical_logical_row(a, current_physical, opt, _prog);

    const CursorLineInfo cursor =
        make_cursor_line_info(current_physical, logical_row);

    print_summary(printed, opt, natural_spec, cursor);
}
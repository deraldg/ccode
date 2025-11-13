// src/cli/cmd_formula.cpp — FORMULA / "?" <expr>
#include "xbase.hpp"
#include "cli_comment.hpp"

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstring>

// --- DotTalk++ expression engine ---
#include "dottalk/expr/api.hpp"
#include "dottalk/expr/ast.hpp"
#include "dottalk/expr/eval.hpp"

using namespace xbase;

namespace {

// ------- small helpers -------
static std::string trim_local(std::string s){
    auto issp=[](unsigned char c){return c==' '||c=='\t'||c=='\r'||c=='\n';};
    while(!s.empty()&&issp((unsigned char)s.front())) s.erase(s.begin());
    while(!s.empty()&&issp((unsigned char)s.back()))  s.pop_back();
    return s;
}
static std::string up(std::string s){
    std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return (char)std::toupper(c);});
    return s;
}

// --- DBF header/field scanning (same pattern used in REPLACE) ---
struct FieldMeta { std::string name; char type{}; int length{0}; int decimal{0}; size_t offset{0}; };

static bool read_header_and_fields(const DbArea& a, HeaderRec& hdr, std::vector<FieldMeta>& metas){
    std::ifstream in(a.name(), std::ios::binary); if(!in) return false;
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr)); if(!in) return false;
    metas.clear(); size_t offset=0;
    for(;;){
        FieldRec fr{}; in.read(reinterpret_cast<char*>(&fr), sizeof(fr)); if(!in) return false;
        if((unsigned char)fr.field_name[0]==0x0D) break;
        char namebuf[12]; std::memcpy(namebuf, fr.field_name, 11); namebuf[11]='\0';
        FieldMeta m; m.name=trim_local(std::string(namebuf)); m.type=fr.field_type;
        m.length=(int)fr.field_length; m.decimal=(int)fr.decimal_places; m.offset=offset;
        metas.push_back(m); offset+= (size_t)m.length; if(metas.size()>1024) break;
    }
    return true;
}

static int find_field(const std::vector<FieldMeta>& metas, const std::string& nameUp){
    for(size_t i=0;i<metas.size();++i) if(up(metas[i].name)==nameUp) return (int)i; return -1;
}

// numeric-first evaluation (mirrors REPLACE)
static double eval_number_or_bool(const std::unique_ptr<dottalk::expr::Expr>& e,
                                  const dottalk::expr::RecordView& rv){
    using namespace dottalk::expr;
    if (auto ar = dynamic_cast<const Arith*>(e.get()))      return ar->evalNumber(rv);
    if (auto ln = dynamic_cast<const LitNumber*>(e.get()))  return ln->v;
    if (auto fr = dynamic_cast<const FieldRef*>(e.get())){
        if (auto n = rv.get_field_num(fr->name)) return *n;
        return 0.0;
    }
    return e->eval(rv) ? 1.0 : 0.0;
}

// format doubles like your shell does (no trailing zeros)
static std::string fmt_double(double v){
    std::ostringstream oss;
    oss.setf(std::ios::fixed); oss.precision(12);
    oss << v;
    auto s = oss.str();
    // trim trailing zeros and possible trailing dot
    while (!s.empty() && s.back()=='0') s.pop_back();
    if (!s.empty() && s.back()=='.') s.pop_back();
    if (s.empty()) s = "0";
    return s;
}

} // namespace

void cmd_FORMULA(DbArea& a, std::istringstream& in)
{
    std::string expr;
    std::getline(in, expr);
    expr = cliutil::strip_inline_comments(trim_local(expr));

    if (expr.empty()) {
        std::cout << "Usage: FORMULA <expr>\n";
        return;
    }

    // Build a RecordView only if a valid row is selected; otherwise, still allow pure constants.
    dottalk::expr::RecordView rv;
    HeaderRec hdr{}; std::vector<FieldMeta> metas;
    std::streamoff recStart = 0;

    const bool hasRow = a.isOpen() && a.recno()>0 && a.recno()<=a.recCount()
                      && read_header_and_fields(a, hdr, metas);

    if (hasRow) {
        const long rec = a.recno();
        recStart = (std::streamoff)hdr.data_start
                 + (std::streamoff)((rec-1)*hdr.cpr)
                 + (std::streamoff)1;

        rv.get_field_str = [&](std::string_view nm)->std::string{
            int idx = find_field(metas, up(std::string(nm)));
            if (idx<0) return {};
            const auto& fm = metas[(size_t)idx];
            std::ifstream inF(a.name(), std::ios::binary);
            if (!inF) return {};
            std::string cell((size_t)fm.length, ' ');
            inF.seekg(recStart + (std::streamoff)fm.offset, std::ios::beg);
            inF.read(cell.data(), (std::streamsize)cell.size());
            auto s = cell;
            while(!s.empty()&&std::isspace((unsigned char)s.front())) s.erase(s.begin());
            while(!s.empty()&&std::isspace((unsigned char)s.back()))  s.pop_back();
            return s;
        };
        rv.get_field_num = [&](std::string_view nm)->std::optional<double>{
            int idx = find_field(metas, up(std::string(nm)));
            if (idx<0) return std::nullopt;
            const auto& fm = metas[(size_t)idx];
            std::ifstream inF(a.name(), std::ios::binary);
            if (!inF) return std::nullopt;
            std::string cell((size_t)fm.length, ' ');
            inF.seekg(recStart + (std::streamoff)fm.offset, std::ios::beg);
            inF.read(cell.data(), (std::streamsize)cell.size());
            auto s = cell;
            while(!s.empty()&&std::isspace((unsigned char)s.front())) s.erase(s.begin());
            while(!s.empty()&&std::isspace((unsigned char)s.back()))  s.pop_back();
            try {
                if (s.empty()) return std::nullopt;
                size_t used=0; double v=std::stod(s, &used);
                if (used==s.size()) return v;
            } catch(...) {}
            if (fm.type=='L'){
                if (!s.empty()){
                    char c=(char)std::toupper((unsigned char)s[0]);
                    if (c=='T') return 1.0;
                    if (c=='F') return 0.0;
                }
                return 0.0;
            }
            return std::nullopt;
        };
    } else {
        // No open row: constants and pure functions will still work; field refs return empty/null.
        rv.get_field_str = [](std::string_view)->std::string { return {}; };
        rv.get_field_num = [](std::string_view)->std::optional<double> { return std::nullopt; };
    }

    try {
        auto cr = dottalk::expr::compile_where(expr);
        if (!cr.program) {
            std::cout << "FORMULA error: " << cr.error << "\n";
            return;
        }
        // Try numeric first; if original AST isn’t numeric but boolean, show .T./.F.
        using namespace dottalk::expr;
        bool printed = false;
        if (dynamic_cast<Arith*>(cr.program.get()) ||
            dynamic_cast<FieldRef*>(cr.program.get()) ||
            dynamic_cast<LitNumber*>(cr.program.get())) {
            double num = eval_number_or_bool(cr.program, rv);
            std::cout << fmt_double(num) << "\n";
            printed = true;
        }
        if (!printed) {
            bool b = cr.program->eval(rv);
            std::cout << (b ? ".T." : ".F.") << "\n";
        }
    } catch (const std::exception& e) {
        std::cout << "FORMULA error: " << e.what() << "\n";
    } catch (...) {
        std::cout << "FORMULA error: evaluation failed.\n";
    }
}

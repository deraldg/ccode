// src/cli/cmd_sbr.cpp
// -----------------------------------------------------------------------------
// Simple Browser ("workspace" browser) — order-aware (INX + CNX), with an
// interactive record editor session.
//
// This file deliberately stays *single-table* and *DBF-centric*.
// Multi-table / relational / tuple-stacking lives elsewhere (your new
// WORKSPACE browser command, formerly "Super Browser").
//
// Usage (non-interactive):
//   WORKSPACE [FOR <expr>] [RAW|PRETTY] [PAGE <n>] [ALL] [TOP|BOTTOM]
//             [START KEY <literal>] [QUIET]
//
// Usage (interactive editor session):
//   WORKSPACE ... [EDIT|SESSION]
//
// Interactive keys:
//   N/P  next/prev   |  E edit field   | SAVE/CANCEL
//   DEL/RECALL       |  G <recno>      | CF (CHECK FOR)
//   R refresh order  |  ? help         | Q quit
//
// Notes:
// - Respects active order (INX/CNX via orderstate + order_nav_detail).
// - START KEY uses existing SEEK (respects current order).
// - FOR <expr> uses where_eval::compile_where_expr_cached + run_program.
// - Deleted rows are hidden by default (future: wire to SET DELETED).
// - Interactive edit uses DbArea::set(1-based) + writeCurrent(); staging per-record.
// - After SAVE/DEL/RECALL, we rebuild the order vector and re-sync the cursor.
// -----------------------------------------------------------------------------

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "xbase.hpp"
#include "xbase_field_getters.hpp"
#include "cli/where_eval_shared.hpp"
#include "cli/order_state.hpp"
#include "cli/order_nav.hpp"
#include "xindex/order_display.hpp"   // namespace orderdisplay { std::string summarize(const xbase::DbArea&) }
#include "index_summary.hpp"
#include "value_normalize.hpp"        // normalize_for_compare (pre-save validation)

using std::string;
using std::vector;

using dottalk::IndexSummary;
using namespace util; // normalize_for_compare

// Reuse existing commands
void cmd_SEEK(xbase::DbArea& area, std::istringstream& in);
void cmd_RECALL(xbase::DbArea& area, std::istringstream& in);

namespace {

// simple uppercase
std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

const char* kind_str(IndexSummary::OrderKind k) {
    switch (k) {
        case IndexSummary::OrderKind::Ascending:  return "ASCEND";
        case IndexSummary::OrderKind::Descending: return "DESCEND";
        default:                                  return "PHYSICAL";
    }
}

bool is_open(const xbase::DbArea* a) {
    if (!a) return false;
    try { return a->isOpen() && !a->filename().empty(); }
    catch (...) { return false; }
}

} // namespace

namespace {

static inline string up(const string& s){ string t=s; for(auto& c:t) c=(char)std::toupper((unsigned char)c); return t; }
static inline bool ieq(const string& a,const string& b){ return up(a)==up(b); }

static vector<string> tokenize(std::istringstream& in){
    vector<string> out; string tok; while(in>>tok) out.push_back(tok); return out;
}

static bool parse_int(const string& s,int& out){
    if(s.empty()) return false; char* e=nullptr; long v=strtol(s.c_str(),&e,10);
    if(e==s.c_str()||*e!='\0') return false; if(v<INT_MIN||v>INT_MAX) return false; out=(int)v; return true;
}

static string extract_for_expr(const vector<string>& t){
    int n=(int)t.size(), i=-1; for(int k=0;k<n;++k) if(ieq(t[k],"FOR")){i=k;break;}
    if(i<0||i+1>=n) return "";
    auto stop=[](const string& s){
        return ieq(s,"RAW")||ieq(s,"PRETTY")||ieq(s,"PAGE")||ieq(s,"ALL")
            ||ieq(s,"TOP")||ieq(s,"BOTTOM")||ieq(s,"START")||ieq(s,"QUIET")
            ||ieq(s,"EDIT")||ieq(s,"SESSION");
    };
    std::ostringstream o; for(int k=i+1;k<n;++k){ if(stop(t[k])) break; if(k>i+1) o<<' '; o<<t[k]; } return o.str();
}

static string extract_start_key(const vector<string>& t){
    for(size_t i=0;i+2<t.size();++i) if(ieq(t[i],"START")&&ieq(t[i+1],"KEY")) return t[i+2];
    return "";
}

static void print_tuple_pretty(xbase::DbArea& db){
    const auto& defs=db.fields(); std::ostringstream line; line<<"; TUPLE: ";
    for(size_t i=0;i<defs.size();++i){ if(i) line<<" | "; line<<db.get((int)i+1); }
    std::cout<<line.str()<<"\n";
}
static void print_tuple_raw(xbase::DbArea& db){
    const auto& defs=db.fields(); std::ostringstream line; for(size_t i=0;i<defs.size();++i) line<<db.get((int)i+1);
    std::cout<<line.str()<<"\n";
}

static bool hide_deleted_by_default(){ return true; }
static bool is_deleted_and_hidden(xbase::DbArea& db){ return hide_deleted_by_default() && db.isDeleted(); }

static bool more_prompt(bool quiet){
    if(quiet) return true;
    std::cout<<"-- More -- (Enter to continue, Q to quit) "; std::cout.flush();
    std::string line; if(!std::getline(std::cin,line)) return false;
    if(!line.empty() && (line[0]=='q'||line[0]=='Q')) return false;
    return true;
}

// Use the proven utility banner that already shows INX/CNX details (including tag)
static std::string order_banner(xbase::DbArea& a){
    return orderdisplay::summarize(a);
}

// Active order snapshot (recno list + direction)
struct Ordered {
    vector<uint32_t> recnos;   // 1-based recnos in active order
    bool asc = true;
    bool has = false;
};

static Ordered build_order_vector(xbase::DbArea& area){
    Ordered o;
    o.has = orderstate::hasOrder(area);
    o.asc = o.has ? orderstate::isAscending(area) : true;
    if(!o.has) return o;

    if(orderstate::isCnx(area)){
        const std::string tag = orderstate::activeTag(area);
        order_nav_detail::build_cnx_recnos_from_db(area, tag, o.recnos);
    } else {
        const std::string ord_path = orderstate::orderName(area);
        if(!ord_path.empty()){
            order_nav_detail::load_inx_recnos(ord_path, area.recCount(), o.recnos);
        }
    }
    return o;
}

static bool position_to_recno(xbase::DbArea& area, uint32_t rn){
    if(!area.gotoRec((int32_t)rn)) return false;
    return area.readCurrent();
}

static void print_help_inline(){
    std::cout <<
        "Commands:\n"
        "  N / P          - Next / Previous (order-aware, respects FOR)\n"
        "  G <recno>      - Go to record number\n"
        "  R              - Refresh (rebuild active order vector + re-sync cursor)\n"
        "  E [<field> [WITH <value>]]  - Edit current record (prompt if <field> omitted)\n"
        "  SAVE / CANCEL  - Commit or discard staged edits\n"
        "  DEL / RECALL   - Mark deleted / Undelete current record\n"
        "  CF / CHECK FOR <expr> - Evaluate FOR on current record (TRUE/FALSE)\n"
        "  STATUS         - Reprint status line\n"
        "  H / ?          - Help\n"
        "  Q              - Quit\n";
}

static void banner_order_updated(){
    std::cout<<"[ORDER UPDATED] Active order rebuilt and cursor re-synced.\n";
}

// --- New helpers: CHECK FOR + pre-save validation ---

static bool browse_eval_for_on_current(xbase::DbArea& area, const std::string& expr, bool& ok) {
    ok = false;
    if (expr.empty()) return false;
    auto prog = where_eval::compile_where_expr_cached(expr);
    if (!prog || !prog->plan) return false;
    if (!area.readCurrent()) return false;
    ok = true;
    return where_eval::run_program(*prog->plan, area);
}

static bool browse_validate_staged_before_save(xbase::DbArea& area,
                                               const std::map<int, std::string>& staged) {
    if (staged.empty()) return true;
    const auto& defs = area.fields();
    for (const auto& kv : staged) {
        int idx = kv.first;
        if (idx < 1 || idx > (int)defs.size()) {
            std::cout << "SAVE blocked: invalid field index #" << idx << "\n";
            return false;
        }
        const auto& f = defs[(size_t)idx - 1];
        const char t  = f.type;
        const int  L  = (int)f.length;
        const int  D  = (int)f.decimals;

        auto norm = normalize_for_compare(t, L, D, kv.second);
        if (!norm) {
            std::cout << "SAVE blocked: invalid value for " << f.name << "\n";
            return false;
        }
        if ((t=='C' || t=='N') && (int)norm->size() > L) {
            std::cout << "SAVE blocked: value too wide for " << f.name
                      << " (" << norm->size() << " > " << L << ")\n";
            return false;
        }
    }
    return true;
}

} // namespace

void cmd_SBR(xbase::DbArea& area, std::istringstream& in)
{
    const auto toks=tokenize(in);

    bool want_raw=false, list_all=false, quiet=false, interactive=false;
    int page_size=20;
    enum class StartPos{TOP,BOTTOM} start_pos=StartPos::TOP;

    const string for_expr=extract_for_expr(toks);
    const string start_key=extract_start_key(toks);

    for(size_t i=0;i<toks.size();++i){
        const string t=up(toks[i]);
        if(t=="RAW"){ want_raw=true; continue; }
        if(t=="PRETTY"){ want_raw=false; continue; }
        if(t=="ALL"){ list_all=true; continue; }
        if(t=="TOP"){ start_pos=StartPos::TOP; continue; }
        if(t=="BOTTOM"){ start_pos=StartPos::BOTTOM; continue; }
        if(t=="QUIET"){ quiet=true; continue; }
        if((t=="PAGE") && i+1<toks.size()){ int n=0; if(parse_int(toks[i+1],n) && n>0){ page_size=n; ++i; } }
        if(t=="EDIT" || t=="SESSION"){ interactive=true; continue; }
    }

    if(!quiet){
        std::cout<<"Entered WORKSPACE mode "<<(interactive?"(interactive)":"(read-only)")<<".\n";
        std::cout<<"ORDER: "<<order_banner(area)<<"\n";
        std::cout<<"Format: "<<(want_raw?"RAW":"PRETTY")
                 <<" | Start: "<<(start_pos==StartPos::TOP?"TOP":"BOTTOM")
                 <<" | Page: "<<page_size
                 <<(list_all?" | ALL":"")
                 <<(for_expr.empty()?"":" | FOR: "+for_expr)
                 <<(start_key.empty()?"":" | START KEY: "+start_key)
                 <<"\n\n";
    }

    // Compile FOR
    std::shared_ptr<const where_eval::CacheEntry> prog;
    if(!for_expr.empty()){
        prog = where_eval::compile_where_expr_cached(for_expr);
        if(!prog){ if(!quiet) std::cout<<"Invalid FOR expression.\n"; return; }
    }

    // Starting position
    if(start_pos==StartPos::TOP) area.top(); else area.bottom();

    // START KEY uses existing SEEK (respects active order)
    if(!start_key.empty()){
        std::istringstream seek_args(start_key);
        cmd_SEEK(area, seek_args);
    }

    auto print_tuple = [&](xbase::DbArea& db){ if(want_raw) print_tuple_raw(db); else print_tuple_pretty(db); };

    // Build order vector (if any)
    auto ord = build_order_vector(area);

    // Helper: visibility & filter match
    auto visible_match = [&](xbase::DbArea& db)->bool{
        if(is_deleted_and_hidden(db)) return false;
        if(prog && !where_eval::run_program(*prog->plan, db)) return false;
        return true;
    };

    // --------- Non-interactive listing ---------
    if(!interactive){
        const long limit = list_all ? std::numeric_limits<long>::max() : page_size;
        long shown_this_page=0, total=0;

        if(ord.has && !ord.recnos.empty()){
            auto emit = [&](uint32_t rn)->bool{
                if(!position_to_recno(area, rn)) return true;
                if(!visible_match(area)) return true;
                print_tuple(area);
                ++shown_this_page; ++total;
                if(shown_this_page>=limit && !list_all){ shown_this_page=0; return more_prompt(quiet); }
                return true;
            };

            if(ord.asc){
                if(start_pos==StartPos::TOP){
                    for(size_t i=0;i<ord.recnos.size();++i) if(!emit(ord.recnos[i])) break;
                } else {
                    for(size_t k=ord.recnos.size(); k-- > 0; ) if(!emit(ord.recnos[k])) break;
                }
            } else { // DESC
                if(start_pos==StartPos::TOP){
                    for(size_t k=ord.recnos.size(); k-- > 0; ) if(!emit(ord.recnos[k])) break;
                } else {
                    for(size_t i=0;i<ord.recnos.size();++i) if(!emit(ord.recnos[i])) break;
                }
            }
            if(!quiet) std::cout<< total <<" record(s) listed.\n";
            return;
        }

        // Physical fallback
        const int step = (start_pos==StartPos::TOP)? +1 : -1;
        while(true){
            const int rn=area.recno(), rct=area.recCount();
            if(rn<1 || rn>rct) break;
            if(visible_match(area)){
                print_tuple(area);
                ++shown_this_page; ++total;
                if(shown_this_page>=limit && !list_all){ shown_this_page=0; if(!more_prompt(quiet)) break; }
            }
            if(!area.skip(step)) break;
        }
        if(!quiet) std::cout<< total <<" record(s) listed.\n";
        return;
    }

    // --------- Interactive session (EDIT/SESSION) ---------

    // Find index of current recno in order vector
    auto find_index_of_recno = [&](const vector<uint32_t>& v, uint32_t rn){
        for(size_t i=0;i<v.size();++i) if(v[i]==rn) return (int)i;
        return -1;
    };

    int cur_idx = -1;
    if(ord.has && !ord.recnos.empty()){
        cur_idx = find_index_of_recno(ord.recnos, (uint32_t)area.recno());
        if(cur_idx<0){
            cur_idx = (start_pos==StartPos::TOP) ? 0 : (int)ord.recnos.size()-1;
            position_to_recno(area, ord.recnos[(size_t)cur_idx]);
        }
    }

    // Rebuild order vector and re-sync cursor (Option 2)
    auto rebuild_and_resync_order = [&](){
        if(!orderstate::hasOrder(area)) return; // nothing to do when physical
        const int cur_rec = area.recno();
        ord = build_order_vector(area);
        if(!ord.recnos.empty()){
            int idx = find_index_of_recno(ord.recnos, (uint32_t)cur_rec);
            if(idx<0){
                bool moved=false;
                int try_idx_first  = ord.asc ? 0 : (int)ord.recnos.size()-1;
                int try_idx_second = ord.asc ? (int)ord.recnos.size()-1 : 0;

                for(int candidate : { try_idx_first, try_idx_second }){
                    if(position_to_recno(area, ord.recnos[(size_t)candidate]) && visible_match(area)){
                        cur_idx = candidate;
                        moved = true;
                        break;
                    }
                }
                if(!moved){
                    for(size_t i=0;i<ord.recnos.size();++i){
                        if(position_to_recno(area, ord.recnos[i]) && visible_match(area)){
                            cur_idx = (int)i;
                            moved = true;
                            break;
                        }
                    }
                }
                if(!moved){
                    cur_idx = -1;
                }
            }else{
                cur_idx = idx;
                position_to_recno(area, ord.recnos[(size_t)cur_idx]);
            }
        }
        banner_order_updated();
    };

    // Staging map: field index (1-based) -> staged value
    std::map<int,string> staged;
    auto dirty = [&](){ return !staged.empty(); };

    auto print_status = [&](){
        std::string tab;
        try {
            tab = area.name();
            if (tab.empty()) tab = area.filename();
        } catch (...) {
            tab.clear();
        }
        if (tab.empty()) tab = "(no table)";

        std::cout
            << "Table: " << tab
            << " | Recs: " << area.recCount()
            << " | Recno: " << area.recno()
            << " | Order: " << order_banner(area)
            << (dirty() ? " | *DIRTY*" : "")
            << (area.isDeleted() ? " | [DELETED]" : "")
            << "\n";
    };

    auto show_current = [&](){
        if(!area.readCurrent()){ std::cout<<"(unreadable record)\n"; return; }
        if(want_raw) print_tuple_raw(area); else print_tuple_pretty(area);
    };

    auto next_in_order = [&]()->bool{
        if(ord.has && !ord.recnos.empty()){
            int step = ord.asc? +1 : -1;
            if(cur_idx<0){ cur_idx = (ord.asc? 0 : (int)ord.recnos.size()-1); }
            for(;;){
                cur_idx += step;
                if(cur_idx<0 || cur_idx>=(int)ord.recnos.size()) return false;
                if(!position_to_recno(area, ord.recnos[(size_t)cur_idx])) continue;
                if(visible_match(area)) return true;
            }
        } else {
            while(area.skip(+1)){
                if(visible_match(area)) return true;
            }
            return false;
        }
    };
    auto prev_in_order = [&]()->bool{
        if(ord.has && !ord.recnos.empty()){
            int step = ord.asc? -1 : +1;
            if(cur_idx<0){ cur_idx = (ord.asc? 0 : (int)ord.recnos.size()-1); }
            for(;;){
                cur_idx += step;
                if(cur_idx<0 || cur_idx>=(int)ord.recnos.size()) return false;
                if(!position_to_recno(area, ord.recnos[(size_t)cur_idx])) continue;
                if(visible_match(area)) return true;
            }
        } else {
            while(area.skip(-1)){
                if(visible_match(area)) return true;
            }
            return false;
        }
    };

    auto commit_staged = [&]()->bool{
        if(staged.empty()) return true;
        for(auto& kv: staged){
            if(!area.set(kv.first, kv.second)) {
                std::cout<<"Failed to set field #"<<kv.first<<"\n";
                return false;
            }
        }
        if(!area.writeCurrent()){
            std::cout<<"Failed to write record.\n";
            return false;
        }
        staged.clear();
        return true;
    };
    auto discard_staged = [&](){ staged.clear(); };

    auto list_fields = [&](){
        const auto& defs = area.fields();
        std::cout<<"Fields ("<<defs.size()<<")\n";
        for(size_t i=0;i<defs.size();++i){
            const auto& f=defs[i];
            std::cout<<"  "<<(i+1)<<": "<<f.name<<" ["<<f.type<<"]";
            if(f.type=='C' || f.type=='N') std::cout<<" len="<<static_cast<int>(f.length);
            if(f.type=='N' && f.decimals>0) std::cout<<" dec="<<static_cast<int>(f.decimals);
            std::cout<<"  current="<<area.get((int)i+1)<<"\n";
        }
    };

    auto field_index_by_name = [&](const string& name)->int{
        const auto& defs=area.fields(); string needle=up(name);
        for(size_t i=0;i<defs.size();++i) if(up(defs[i].name)==needle) return (int)i+1;
        return 0;
    };

    auto edit_field = [&](int field_idx, const string* value_opt)->void{
        const auto& defs=area.fields();
        if(field_idx<1 || field_idx>(int)defs.size()){ std::cout<<"Invalid field.\n"; return; }
        const auto& f = defs[(size_t)field_idx-1];
        string newval;
        if(value_opt){
            newval = *value_opt;
        }else{
            std::cout<<"Enter value for "<<f.name<<" ["<<f.type<<"] (current="<<area.get(field_idx)<<"): ";
            std::getline(std::cin, newval);
        }
        staged[field_idx] = newval;
        std::cout<<"Staged "<<f.name<<" = "<<newval<<"\n";
    };

    // Initial display
    print_status();
    show_current();
    print_help_inline();

    // Command loop
    for(;;){
        std::cout<<"WS> "; std::cout.flush();
        string line; if(!std::getline(std::cin,line)) break;
        std::istringstream lin(line);
        string cmd; lin >> cmd; if(cmd.empty()) continue;
        string CMD = up(cmd);

        if(CMD=="Q" || CMD=="QUIT"){
            if(dirty()){
                std::cout<<"You have staged changes. SAVE or CANCEL first.\n";
                continue;
            }
            break;
        }
        if(CMD=="H" || CMD=="?"){
            print_help_inline();
            continue;
        }

        if(CMD=="STATUS"){
            print_status();
            continue;
        }

        if(CMD=="CF" || CMD=="CHECK"){
            // Accept: CHECK FOR <expr>   or   CF <expr>
            string maybeFor; string expr;
            std::getline(lin, expr);
            if(!expr.empty() && expr[0]==' ') expr.erase(0,1);
            if(up(expr.size()>=4 ? expr.substr(0,4) : "")=="FOR "){
                expr = expr.substr(4);
                if(!expr.empty() && expr[0]==' ') expr.erase(0,1);
            }
            bool ok=false;
            bool truth = browse_eval_for_on_current(area, expr, ok);
            if(!ok) std::cout<<"CHECK FOR: invalid or unreadable.\n";
            else    std::cout<<"CHECK FOR => "<<(truth ? "TRUE" : "FALSE")<<"\n";
            continue;
        }

        if(CMD=="R" || CMD=="REFRESH"){
            if(dirty()){
                std::cout<<"*DIRTY* ? SAVE or CANCEL before refresh.\n";
                continue;
            }
            rebuild_and_resync_order();
            print_status();
            show_current();
            continue;
        }
        if(CMD=="N"){
            if(dirty()){ std::cout<<"*DIRTY* ? SAVE or CANCEL before moving.\n"; continue; }
            if(next_in_order()){ print_status(); show_current(); } else { std::cout<<"(end)\n"; }
            continue;
        }
        if(CMD=="P"){
            if(dirty()){ std::cout<<"*DIRTY* ? SAVE or CANCEL before moving.\n"; continue; }
            if(prev_in_order()){ print_status(); show_current(); } else { std::cout<<"(begin)\n"; }
            continue;
        }
        if(CMD=="G"){
            if(dirty()){ std::cout<<"*DIRTY* ? SAVE or CANCEL before moving.\n"; continue; }
            string rn_s; lin>>rn_s; int rn=0;
            if(!parse_int(rn_s,rn) || rn<1){ std::cout<<"Usage: G <recno>\n"; continue; }
            if(!position_to_recno(area,(uint32_t)rn)){ std::cout<<"Bad recno.\n"; continue; }
            if(ord.has && !ord.recnos.empty()){
                int idx=-1; for(size_t i=0;i<ord.recnos.size();++i) if(ord.recnos[i]==(uint32_t)rn){ idx=(int)i; break; }
                if(idx>=0) { /* keep cur_idx implicit; navigation handles it */ }
            }
            print_status(); show_current();
            continue;
        }
        if(CMD=="E"){
            // E <field> [WITH <value>]
            string field; lin>>field;
            if(field.empty()){
                list_fields();
                std::cout<<"Field name or #? ";
                string pick; std::getline(std::cin, pick);
                int fidx=0;
                if(parse_int(pick,fidx)){
                    // numeric index
                }else{
                    fidx = field_index_by_name(pick);
                }
                if(fidx<=0){ std::cout<<"Invalid selection.\n"; continue; }
                edit_field(fidx, nullptr);
            }else{
                int fidx=0;
                if(parse_int(field,fidx)==false){
                    fidx = field_index_by_name(field);
                }
                if(fidx<=0){ std::cout<<"Unknown field.\n"; continue; }
                string with; lin>>with;
                string value;
                if(!with.empty() && up(with)=="WITH"){
                    std::getline(lin, value);
                    if(!value.empty() && value[0]==' ') value.erase(0,1);
                }else{
                    value.clear();
                    std::cout<<"Enter value: ";
                    std::getline(std::cin,value);
                }
                edit_field(fidx, &value);
            }
            continue;
        }
        if(CMD=="SAVE"){
            // New: pre-save validation
            if(!browse_validate_staged_before_save(area, staged)){
                continue; // blocked; user must adjust staged values or CANCEL
            }
            if(commit_staged()){
                std::cout<<"Saved.\n";
                rebuild_and_resync_order();  // Option 2
                show_current();
            }
            continue;
        }
        if(CMD=="CANCEL"){
            discard_staged();
            std::cout<<"Canceled staged edits.\n";
            continue;
        }
        if(CMD=="DEL"){
            if(dirty()){ std::cout<<"*DIRTY* ? SAVE or CANCEL first.\n"; continue; }
            if(area.deleteCurrent()){
                std::cout<<"Deleted.\n";
                rebuild_and_resync_order();  // Option 2
                if(!next_in_order()) std::cout<<"(end)\n";
                else { print_status(); show_current(); }
            }else{
                std::cout<<"Delete failed.\n";
            }
            continue;
        }
        if(CMD=="RECALL" || CMD=="UNDELETE"){
            if(dirty()){ std::cout<<"*DIRTY* ? SAVE or CANCEL first.\n"; continue; }
            bool wasDel = area.isDeleted();
            std::istringstream none;
            cmd_RECALL(area, none);             // call the existing command handler
            bool ok = wasDel && !area.isDeleted();
            if(ok){
                std::cout<<"Recalled.\n";
                rebuild_and_resync_order();     // Option 2
                print_status(); show_current();
            }else{
                std::cout<<"Recall failed.\n";
            }
            continue;
        }

        std::cout<<"Unknown command. Type ? for help.\n";
    }
}




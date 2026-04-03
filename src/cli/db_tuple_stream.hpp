#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include "tuple_stream.hpp"
#include "tuple_types.hpp"
#include "cli/expr/api.hpp"
#include "cli/expr/ast.hpp"
#include "expr_tuple_glue.hpp"
namespace dottalk {
class DbTupleStream final : public TupleStream {
public:
    explicit DbTupleStream(std::string spec, std::string page_hint = "");
    void top() override;
    void bottom() override;
    void skip(long n) override;
    std::vector<TupleRow> next_page(std::size_t max_rows) override;
    std::string status_line() const override;
    void set_filter_for(std::string expr);
    void set_spec(std::string spec) { spec_ = std::move(spec); top(); }
    std::string current_filter() const { return filter_for_; }
    const std::string& current_spec() const { return spec_; }
    bool goto_pos(long pos);
    bool goto_recno(long r);
    bool is_ordered() const;
    long order_count() const;
    long current_pos() const;
    void set_order_physical();
    void set_order_inx();
    void set_order_cnx();
    std::string current_area_name() const;
private:
    enum class NavMode : uint8_t { Physical = 0, OrderVector = 1 };
    std::string spec_;
    std::string hint_;
    std::string filter_for_;
    std::unique_ptr<dottalk::expr::Expr> filter_prog_;
    NavMode     mode_              = NavMode::Physical;
    long        cur_recno_         = 0;
    long        max_recno_         = 0;
    std::vector<uint32_t> order_recnos_;
    long        order_pos_         = -1;
    long        last_emitted_recno_= 0;
    void refresh_bounds_only();
    void refresh_bounds_and_order();
    bool goto_physical_recno(long r);
    bool goto_order_pos(long p);
    bool goto_recno_internal(long r);
    bool step(long delta);
    TupleRow build_current_tuple();
    bool passes_filter_on_tuple(const TupleRow& row) const;
    std::string current_order_hint() const;
};
} // namespace dottalk

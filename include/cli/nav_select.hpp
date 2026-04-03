#pragma once

#include <cstdint>

#include "xbase.hpp"
#include "cli/logical_nav.hpp"
#include "cli/order_nav.hpp"
#include "filters/filter_registry.hpp"

namespace cli::navsel {

enum class Mode {
    RawOrder,
    LogicalView,
    AutoByFilter
};

enum class Step {
    First,
    Last,
    Next,
    Prior
};

inline Mode resolve_mode(xbase::DbArea& A, Mode mode)
{
    if (mode == Mode::AutoByFilter) {
        return filter::has_active_filter(&A)
            ? Mode::LogicalView
            : Mode::RawOrder;
    }
    return mode;
}

inline int32_t pick_recno(xbase::DbArea& A,
                          Mode mode,
                          Step step,
                          int32_t from_recno = 0)
{
    if (!A.isOpen()) return 0;

    mode = resolve_mode(A, mode);

    switch (mode) {
    case Mode::LogicalView:
        switch (step) {
        case Step::First:
            return cli::logical_nav::first_recno(A);
        case Step::Last:
            return cli::logical_nav::last_recno(A);
        case Step::Next:
            return cli::logical_nav::next_recno(
                A, from_recno > 0 ? from_recno : A.recno());
        case Step::Prior:
            return cli::logical_nav::prev_recno(
                A, from_recno > 0 ? from_recno : A.recno());
        }
        break;

    case Mode::RawOrder:
    {
        int32_t rn = 0;

        switch (step) {
        case Step::First:
            if (order_first_recno(A, rn)) return rn;
            return (A.recCount() > 0 ? 1 : 0);

        case Step::Last:
            if (order_last_recno(A, rn)) return rn;
            return (A.recCount() > 0 ? A.recCount() : 0);

        case Step::Next:
        {
            const int32_t save = A.recno();
            const int32_t start = (from_recno > 0 ? from_recno : save);

            if (start <= 0) return 0;
            if (start != save) {
                if (!A.gotoRec(start) || !A.readCurrent()) return 0;
            }

            const bool ok = order_skip(A, +1) || A.skip(+1);
            rn = ok ? A.recno() : 0;

            if (save > 0) {
                (void)A.gotoRec(save);
                (void)A.readCurrent();
            }
            return rn;
        }

        case Step::Prior:
        {
            const int32_t save = A.recno();
            const int32_t start = (from_recno > 0 ? from_recno : save);

            if (start <= 0) return 0;
            if (start != save) {
                if (!A.gotoRec(start) || !A.readCurrent()) return 0;
            }

            const bool ok = order_skip(A, -1) || A.skip(-1);
            rn = ok ? A.recno() : 0;

            if (save > 0) {
                (void)A.gotoRec(save);
                (void)A.readCurrent();
            }
            return rn;
        }
        }
    }

    case Mode::AutoByFilter:
        break;
    }

    return 0;
}

} // namespace cli::navsel

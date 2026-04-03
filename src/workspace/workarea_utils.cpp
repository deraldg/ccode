#include "workspace/workarea_utils.hpp"
#include "workareas.hpp"

#include <sstream>
#include <vector>

namespace workareas {

std::size_t open_count() {
    const auto areas = all();
    const std::size_t n = areas.count();

    std::size_t open = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const auto wa = areas[i];
        if (wa.is_open()) {
            ++open;
        }
    }
    return open;
}

std::string occupied_desc() {
    const auto areas = all();
    const std::size_t n = areas.count();

    std::vector<std::size_t> slots;
    slots.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        const auto wa = areas[i];
        if (wa.is_open()) {
            slots.push_back(i);
        }
    }

    if (slots.empty()) {
        return "{}";
    }

    std::ostringstream out;
    out << "{";

    std::size_t run_start = slots[0];
    std::size_t run_end   = slots[0];

    auto flush_run = [&](std::size_t a, std::size_t b) {
        if (a == b) out << a;
        else        out << a << ".." << b;
    };

    bool first = true;

    for (std::size_t i = 1; i < slots.size(); ++i) {
        if (slots[i] == run_end + 1) {
            run_end = slots[i];
            continue;
        }

        if (!first) out << ",";
        flush_run(run_start, run_end);
        first = false;

        run_start = slots[i];
        run_end   = slots[i];
    }

    if (!first) out << ",";
    flush_run(run_start, run_end);

    out << "}";
    return out.str();
}

} // namespace workareas

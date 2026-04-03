#include "workareas.hpp"

#include <iomanip>
#include <iostream>
#include <string>

// Provided by shell.cpp (C linkage there)
extern "C" xbase::XBaseEngine* shell_engine();

namespace {
    thread_local std::string g_name_buffer;

    inline xbase::XBaseEngine* engine_ptr() noexcept {
        return shell_engine();
    }

    inline bool slot_in_range(std::size_t slot0) noexcept {
        return slot0 < static_cast<std::size_t>(xbase::MAX_AREA);
    }
}

namespace workareas {

bool WorkArea::valid() const noexcept {
    return engine_ptr() != nullptr && slot_in_range(slot0_);
}

xbase::DbArea* WorkArea::db() const noexcept {
    auto* eng = engine_ptr();
    if (!eng) return nullptr;
    if (!slot_in_range(slot0_)) return nullptr;

    try {
        return &eng->area(static_cast<int>(slot0_));
    } catch (...) {
        return nullptr;
    }
}

bool WorkArea::is_open() const noexcept {
    auto* a = db();
    if (!a) return false;

    try {
        return a->isOpen();
    } catch (...) {
        return false;
    }
}

std::string WorkArea::logical_name() const {
    auto* a = db();
    if (!a) return {};

    try {
        if (!a->isOpen()) return {};
        return a->name();
    } catch (...) {
        return {};
    }
}

std::string WorkArea::file_name() const {
    auto* a = db();
    if (!a) return {};

    try {
        if (!a->isOpen()) return {};
        return a->filename();
    } catch (...) {
        return {};
    }
}

std::string WorkArea::label() const {
    std::string s = logical_name();
    if (!s.empty()) return s;

    s = file_name();
    if (!s.empty()) return s;

    return {};
}

bool WorkArea::has_label() const {
    return !label().empty();
}

xbase::AreaKind WorkArea::kind() const noexcept {
    auto* a = db();
    if (!a) return xbase::AreaKind::Unknown;

    try {
        return a->kind();
    } catch (...) {
        return xbase::AreaKind::Unknown;
    }
}

bool WorkArea::supports(xbase::AreaCapability cap) const noexcept {
    auto* a = db();
    if (!a) return false;

    try {
        return a->supports(cap);
    } catch (...) {
        return false;
    }
}

std::size_t WorkAreaSet::count() const noexcept {
    auto* eng = engine_ptr();
    return eng ? static_cast<std::size_t>(xbase::MAX_AREA) : 0;
}

WorkArea WorkAreaSet::operator[](std::size_t slot0) const noexcept {
    return WorkArea(slot0);
}

WorkArea WorkAreaSet::current() const noexcept {
    return WorkArea(current_slot());
}

std::size_t WorkAreaSet::current_slot() const noexcept {
    auto* eng = engine_ptr();
    if (!eng) return count();

    try {
        const int cur = eng->currentArea();
        if (cur < 0) return count();
        return static_cast<std::size_t>(cur);
    } catch (...) {
        return count();
    }
}

void WorkAreaSet::print(std::ostream& os) const {
    const std::size_t n = count();
    const std::size_t cur = current_slot();

    os << "  Slot  Cur  Name\n";
    os << "  ----- ---- ------------------------------\n";

    for (std::size_t i = 0; i < n; ++i) {
        WorkArea wa(i);
        if (!wa.is_open()) continue;

        const std::string label = wa.label();
        if (label.empty()) continue;

        os << "  "
           << std::setw(5) << i << " "
           << std::setw(4) << (i == cur ? "*" : "")
           << " " << label << "\n";
    }
}

std::size_t count() {
    return all().count();
}

xbase::DbArea* at(std::size_t slot0) {
    return WorkArea(slot0).db();
}

const char* name(std::size_t slot0) {
    g_name_buffer = WorkArea(slot0).label();
    return g_name_buffer.c_str();
}

std::size_t current_slot() {
    return all().current_slot();
}

void print(std::ostream& os) {
    all().print(os);
}

void show(xbase::DbArea&) {
    print(std::cout);
}

WorkAreaSet all() {
    return WorkAreaSet{};
}

WorkArea current() {
    return all().current();
}

} // namespace workareas

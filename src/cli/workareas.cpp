#include "workareas.hpp"
#include <string>

// Provided by shell.cpp (C linkage there)
extern "C" xbase::XBaseEngine* shell_engine();

namespace {
    thread_local std::string g_label;
}

namespace workareas {

std::size_t count() {
    auto* eng = shell_engine();
    return eng ? static_cast<std::size_t>(xbase::MAX_AREA) : 0;
}

xbase::DbArea* at(std::size_t slot0) {
    auto* eng = shell_engine();
    const std::size_t n = count();
    if (!eng || slot0 >= n) return nullptr;
    try { return &eng->area(static_cast<int>(slot0)); }
    catch (...) { return nullptr; }
}

const char* name(std::size_t slot0) {
    g_label.clear();
    if (auto* a = at(slot0)) {
        if (a->isOpen()) {
            try { auto nm = a->name();     if (!nm.empty()) { g_label = nm; return g_label.c_str(); } } catch (...) {}
            try { auto fn = a->filename(); if (!fn.empty()) { g_label = fn; return g_label.c_str(); } } catch (...) {}
        }
    }
    g_label = std::to_string(static_cast<int>(slot0));
    return g_label.c_str();
}

std::size_t current_slot() {
    auto* eng = shell_engine();
    return eng ? static_cast<std::size_t>(eng->currentArea()) : count();
}

} // namespace workareas

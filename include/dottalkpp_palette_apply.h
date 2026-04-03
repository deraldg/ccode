#pragma once
#include <tvision/app.h>
#include <tvision/views.h>

// Copy a palette into the running app and invalidate for redraw.
inline void applyPalette(TApplication &app, const TPalette &pal) {
    auto &dst = const_cast<TPalette&>(static_cast<const TProgram&>(app).getPalette());
    dst = pal;
    app.invalidate();
}




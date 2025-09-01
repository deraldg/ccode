#pragma once
#include "browse/browse_view_model.hpp"
#include "layout_manager.hpp"
namespace browse {
class IBrowseRenderer {
public:
    virtual ~IBrowseRenderer() = default;
    virtual void render(const BrowseViewModel& vm, const Layout& layout) = 0;
    virtual void invalidate_all() = 0;
    virtual void set_windowed(bool on) = 0;
};
IBrowseRenderer* create_renderer_v2();
} // namespace browse

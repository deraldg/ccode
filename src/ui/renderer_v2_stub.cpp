#include "ui/ibrowse_renderer.hpp"
#include <iostream>
namespace browse {
class RendererV2Stub final : public IBrowseRenderer {
    bool windowed_ = true;
public:
    void render(const BrowseViewModel&, const Layout&) override {
        static int counter=0;
        if (++counter<=1) std::cerr << "[renderer_v2_stub] render() (stub) windowed=" << (windowed_?"true":"false") << "\n";
    }
    void invalidate_all() override {}
    void set_windowed(bool on) override { windowed_ = on; }
};
IBrowseRenderer* create_renderer_v2(){ return new RendererV2Stub(); }
} // namespace browse

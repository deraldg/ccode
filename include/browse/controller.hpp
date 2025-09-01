#pragma once
#include "browse_view_model.hpp"
#include "ui/ibrowse_renderer.hpp"
#include "data/irecord_gateway.hpp"
#include "input/iinput_source.hpp"
namespace browse {
class Controller {
public:
    Controller(IRecordGateway& gw, IBrowseRenderer& r, IInputSource& in);
    bool tick();
    void init_vm(int termW, int termH, bool windowedDefault);
private:
    IRecordGateway& gw_;
    IBrowseRenderer& r_;
    IInputSource& in_;
    BrowseViewModel vm_;
};
} // namespace browse

#include "browse/controller.hpp"
#include "ui/layout_manager.hpp"
#include <algorithm>
namespace browse {
Controller::Controller(IRecordGateway& gw, IBrowseRenderer& r, IInputSource& in)
: gw_(gw), r_(r), in_(in) {}
void Controller::init_vm(int termW, int termH, bool windowedDefault) {
    vm_.windowed = windowedDefault;
    vm_.leftPadding = 1;
    vm_.columns.clear();
    for (auto& n : gw_.column_names()) { Column c; c.name=n; c.width= (int)std::max<size_t>(6, n.size()+1); vm_.columns.push_back(c); }
    vm_.totalRows = gw_.total_rows();
    vm_.vp = {0, std::max(0, termH-3), 0, (int)vm_.columns.size()};
    vm_.selected = 0;
    vm_.status = "Browsetui V2 (stub) — read-only pilot";
}
bool Controller::tick() {
    Command cmd = in_.next();
    switch (cmd) {
    case Command::Quit: return false;
    case Command::NavigateUp:   if (vm_.selected>0) vm_.selected--; break;
    case Command::NavigateDown: vm_.selected++; break;
    case Command::PageUp:       vm_.selected = std::max(0, vm_.selected - std::max(1, vm_.vp.visibleRows-1)); break;
    case Command::PageDown:     vm_.selected += std::max(1, vm_.vp.visibleRows-1); break;
    case Command::ToggleWindowed: vm_.windowed = !vm_.windowed; r_.set_windowed(vm_.windowed); break;
    case Command::EditRecord: case Command::SaveRecord: case Command::None: default: break;
    }
    if (vm_.selected < 0) vm_.selected = 0;
    if (vm_.selected >= vm_.totalRows) vm_.selected = std::max(0, vm_.totalRows-1);
    LayoutManager lm; LayoutSettings s; s.windowed=vm_.windowed; s.minInnerMargin=vm_.leftPadding;
    auto L = lm.compute(120, 40, s); // TODO: replace with actual terminal size
    r_.render(vm_, L);
    return true;
}
} // namespace browse

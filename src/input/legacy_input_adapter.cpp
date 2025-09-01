#include "input/iinput_source.hpp"
#include <iostream>
namespace browse {
class LegacyInputAdapter final : public IInputSource {
public:
    Command next() override {
        // TODO: Replace with adapter to your existing InputHandler.
        // Preserve F6 behavior by mapping it to Command::ToggleWindowed (or current binding).
        std::cerr << "[legacy_input_adapter_stub] next()\n";
        return Command::None;
    }
};
IInputSource* create_legacy_input_adapter(){ return new LegacyInputAdapter(); }
} // namespace browse

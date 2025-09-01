#pragma once
namespace browse {
enum class Command { None=0, Quit, NavigateUp, NavigateDown, PageUp, PageDown, ToggleWindowed, EditRecord, SaveRecord };
class IInputSource { public: virtual ~IInputSource() = default; virtual Command next() = 0; };
IInputSource* create_legacy_input_adapter();
} // namespace browse

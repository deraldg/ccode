Browsetui V2 — Interfaces + Stubs (Safe Drop-in)
=================================================
Purpose: Introduce interfaces/stubs so you can wire a safer, faster Browsetui
incrementally without breaking current behavior (incl. your fixed F6).

Files:
  include/browse/browse_flags.hpp
  include/browse/browse_view_model.hpp
  include/ui/layout_manager.hpp
  include/ui/ibrowse_renderer.hpp
  include/input/iinput_source.hpp
  include/data/irecord_gateway.hpp
  src/ui/layout_manager.cpp
  src/ui/renderer_v2_stub.cpp
  src/input/legacy_input_adapter.cpp
  src/data/record_gateway_dbf_stub.cpp
  src/browse/controller.cpp

Feature flags (env):
  BROWSE_TUI_V2=0|1, BROWSE_TUI_ALLOW_WRITE=0|1, BROWSE_TUI_DIFF=0|1, BROWSE_TUI_WINDOWED=0|1

Wiring sketch:
  if (!browse::use_v2()) { run_current_browsetui(); }
  auto gw = std::unique_ptr<browse::IRecordGateway>(browse::create_dbf_gateway_stub());
  auto r  = std::unique_ptr<browse::IBrowseRenderer>(browse::create_renderer_v2());
  auto in = std::unique_ptr<browse::IInputSource>(browse::create_legacy_input_adapter());
  browse::Controller ctl(*gw, *r, *in);
  ctl.init_vm(TERM_W, TERM_H, browse::windowed_default());
  while (ctl.tick()) {}

Notes:
  - Stub gateway/renderer do nothing destructive; read-only pilot path.
  - Keep your legacy InputHandler mapping; adapter preserves F6 behavior.
  - Replace stub gateway with real DBF gateway (Plan A packers) when ready.

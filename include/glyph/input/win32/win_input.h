// glyph/input/win_input.h
//
// Windows console input backend (minimal).
//
// Responsibilities:
//   - Read console input events and translate to core::Event.
//   - Provide raw mode toggles via InputMode.

#pragma once

#include "glyph/core/event.h"
#include "input.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace glyph::input {

  class WinInput final : public Input {
  public:
    WinInput();
    ~WinInput();

    core::Event poll() override;
    core::Event read() override;

    void      set_mode(InputMode mode) override;
    InputMode get_mode() const override;

  private:
    HANDLE    in_            = INVALID_HANDLE_VALUE;
    DWORD     original_mode_ = 0;
    InputMode mode_          = InputMode::None;

    core::Event translate_record(const INPUT_RECORD &rec);
    core::Event translate_key(const KEY_EVENT_RECORD &key);
    core::Event translate_resize(const WINDOW_BUFFER_SIZE_RECORD &sz);

    core::Mod translate_mods(DWORD state) const noexcept;
  };

} // namespace glyph::input
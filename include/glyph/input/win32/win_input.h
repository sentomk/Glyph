// glyph/input/win_input.h
//
// Windows console input backend (minimal).
//
// Responsibilities:
//   - Read console input events and translate to core::Event.
//   - Provide raw mode toggles via InputMode.

#pragma once

#include "glyph/core/event.h"
#include "glyph/input/input.h"

#include <deque>
#include <string>

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
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
    struct CharInput final {
      char32_t ch   = U'\0';
      core::Mod mods = core::Mod::None;
    };

    enum class AnsiState : std::uint8_t {
      Ground,
      Esc,
      Csi,
      Ss3,
    };

    HANDLE    in_            = INVALID_HANDLE_VALUE;
    HANDLE    out_           = INVALID_HANDLE_VALUE;
    DWORD     original_mode_ = 0;
    InputMode mode_          = InputMode::None;

    core::Event translate_record(const INPUT_RECORD &rec);
    void        enqueue_key(const KEY_EVENT_RECORD &key);
    core::Event translate_mouse(const MOUSE_EVENT_RECORD &mouse);
    core::Event translate_resize(const WINDOW_BUFFER_SIZE_RECORD &sz);

    core::Mod translate_mods(DWORD state) const noexcept;

    void process_chars();
    void flush_ansi(bool force);
    void emit_char(char32_t ch, core::Mod mods, bool repeat);
    void emit_key(core::KeyCode code, core::Mod mods, bool repeat);
    void emit_mouse(core::MouseButton button, core::MouseAction action,
                    core::Point pos, core::Mod mods);

    std::deque<CharInput> char_queue_{};
    std::deque<core::Event> pending_{};
    std::u32string params_{};
    AnsiState      ansi_state_ = AnsiState::Ground;
    core::Mod      esc_mods_   = core::Mod::None;
    DWORD          last_button_state_ = 0;
    bool           mouse_sgr_ = false;
    bool           vt_mouse_enabled_ = false;

    void set_vt_mouse(bool enabled);
  };

} // namespace glyph::input

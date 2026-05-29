// glyph/input/detail/vt_decoder.h
//
// Shared VT/ANSI escape-sequence decoder.
//
// Responsibilities:
//   - Translate a stream of code points (already UTF-32) into core::Event.
//   - Handle CSI / SS3 cursor & function keys, SGR mouse, bracketed paste.
//   - Stay platform-agnostic: no OS API, no IO. Both the POSIX and the
//     Win32 (VT mode) backends feed bytes here.
//
// Usage:
//   VtDecoder dec;
//   dec.feed(ch, base_mods);     // push one code point
//   dec.flush();                 // call when input is idle (resolves lone ESC)
//   while (dec.has_event()) auto ev = dec.pop();

#pragma once

#include <cstdint>
#include <deque>
#include <string>

#include "glyph/core/event.h"

namespace glyph::input::detail {

  class VtDecoder final {
  public:
    VtDecoder() = default;

    // Feed one decoded code point plus any modifier the transport already
    // knows about (Win32 supplies Ctrl/Alt/Shift; POSIX passes None).
    void feed(char32_t ch, core::Mod base_mods = core::Mod::None);

    // Force resolution of a pending partial sequence. A lone ESC that is not
    // followed by anything becomes a standalone Esc key.
    void flush(bool force = true);

    [[nodiscard]] bool has_event() const noexcept {
      return !pending_.empty();
    }

    // Pop the next decoded event. Returns monostate when empty.
    [[nodiscard]] core::Event pop();

  private:
    enum class State : std::uint8_t {
      Ground,
      Esc,
      Csi,
      Ss3,
    };

    void handle_ground(char32_t ch, core::Mod mods);
    void step_esc(char32_t ch, core::Mod mods);
    void step_ss3(char32_t ch);
    void step_csi(char32_t ch);
    void finish_sgr_mouse(char32_t final_ch);
    void finish_csi_tilde();

    void emit_char(char32_t ch, core::Mod mods);
    void emit_key(core::KeyCode code, core::Mod mods);
    void emit_mouse(core::MouseButton button, core::MouseAction action,
                    core::Point pos, core::Mod mods);

    std::deque<core::Event> pending_{};
    std::u32string          params_{};
    std::u32string          paste_buf_{};
    State                   state_      = State::Ground;
    core::Mod               esc_mods_   = core::Mod::None;
    bool                    mouse_sgr_  = false;
    bool                    in_paste_   = false;
  };

} // namespace glyph::input::detail

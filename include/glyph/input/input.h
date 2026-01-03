// glyph/input/input.h
//
// Platform-agnostic input interface.
//
// Responsibilities:
//   - Expose a unified event stream for the app layer.
//   - Abstract platform-specific raw input details.

#pragma once

#include <cstdint>
#include "glyph/core/event.h"

namespace glyph::input {

  // Input mode flags.
  enum class InputMode : std::uint8_t {
    None  = 0,
    Raw   = 1 << 0, // no line buffering, immediate key events.
    Mouse = 1 << 1, // enable mouse events
    Paste = 1 << 2, // enable bracketed paste
  };

  constexpr InputMode operator|(InputMode a, InputMode b) noexcept {
    return InputMode(std::uint8_t(a) | std::uint8_t(b));
  }
  constexpr InputMode operator&(InputMode a, InputMode b) noexcept {
    return InputMode(std::uint8_t(a) & std::uint8_t(b));
  }

  // Abstract input source.
  class Input {
  public:
    virtual ~Input() = default;

    // Non-blocking poll. Returns std::monostate if nothing is available.
    virtual core::Event poll() = 0;

    // Blocking read. Always returns a valid event (non-monostate).
    virtual core::Event read() = 0;

    // Enable/disable input modes (raw/mouse/paste).
    virtual void set_mode(InputMode mode) = 0;
    // Query current input mode.
    virtual InputMode get_mode() const = 0;
  };

} // namespace glyph::input
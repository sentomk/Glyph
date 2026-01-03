// glyph/core/event.h
//
// Input event model.
//
// Responsibilities:
//   - Provide a backend-agnostic event representation.
//   - Keep data plain and copyable for easy dispatch.

#pragma once

#include "geometry.h"
#include <cstdint>
#include <string>
#include <variant>
namespace glyph::core {

  // ------------------------------------------------------------
  // Modifier keys (bitmask)
  // ------------------------------------------------------------
  enum class Mod : std::uint8_t {
    None  = 0,
    Shift = 1 << 0,
    Alt   = 1 << 1,
    Ctrl  = 1 << 2,
    Meta  = 1 << 3,
  };

  constexpr Mod operator|(Mod a, Mod b) noexcept {
    return Mod(std::uint8_t(a) | std::uint8_t(b));
  }
  constexpr Mod operator&(Mod a, Mod b) noexcept {
    return Mod(std::uint8_t(a) & std::uint8_t(b));
  }
  constexpr bool has_mod(Mod v, Mod m) noexcept {
    return (v & m) != Mod::None;
  }

  // ------------------------------------------------------------
  // Key events
  // ------------------------------------------------------------
  enum class KeyCode : std::uint16_t {
    Char, // use KeyEvent::ch
    Enter,
    Esc,
    Tab,
    Backspace,
    Delete,
    Insert,
    Home,
    End,
    PageUp,
    PageDown,
    Up,
    Down,
    Left,
    Right,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
  };

  struct KeyEvent final {
    KeyCode  code   = KeyCode::Char;
    char32_t ch     = U'\0'; // valid only when code == Char.
    Mod      mods   = Mod::None;
    bool     repeat = false;
  };

  // ------------------------------------------------------------
  // Mouse events
  // ------------------------------------------------------------
  enum class MouseButton : std::uint8_t {
    Left,
    Middle,
    Right,
    WheelUp,
    WheelDown,
  };
  enum class MouseAction : std::uint8_t {
    Down,
    Up,
    Move,
    Drag,
    Scroll,
  };

  struct MouseEvent final {
    Point       pos{}; // cell coordinates
    MouseButton button = MouseButton::Left;
    MouseAction action = MouseAction::Move;
    Mod         mods   = Mod::None;
  };

  // ------------------------------------------------------------
  // Window/terminal events
  // ------------------------------------------------------------
  struct ResizeEvent final {
    Size size{};
  };
  enum class FocusState : std::uint8_t {
    Gained,
    Lost,
  };

  struct FocusEvent final {
    FocusState state = FocusState::Gained;
  };

  struct PasteEvent final {
    std::u32string text; // UTF-32 for consistency with Cell::ch
  };

  // ------------------------------------------------------------
  // Unified event
  // ------------------------------------------------------------
  using Event = std::variant<
      std::monostate, // None
      KeyEvent,
      MouseEvent,
      ResizeEvent,
      FocusEvent,
      PasteEvent>;

} // namespace glyph::core

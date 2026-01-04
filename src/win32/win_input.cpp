// glyph/input/win_input.cpp
//
// Windows console input backend (minimal).

#include "glyph/input/win32/win_input.h"
#include "glyph/core/event.h"

#include <cassert>
#include <winuser.h>

namespace glyph::input {

  static core::KeyCode vk_to_keycode(WORD vk) {
    using core::KeyCode;
    switch (vk) {
    case VK_RETURN:
      return KeyCode::Enter;
    case VK_ESCAPE:
      return KeyCode::Esc;
    case VK_TAB:
      return KeyCode::Tab;
    case VK_BACK:
      return KeyCode::Backspace;
    case VK_DELETE:
      return KeyCode::Delete;
    case VK_INSERT:
      return KeyCode::Insert;
    case VK_HOME:
      return KeyCode::Home;
    case VK_END:
      return KeyCode::End;
    case VK_PRIOR:
      return KeyCode::PageUp;
    case VK_NEXT:
      return KeyCode::PageDown;
    case VK_UP:
      return KeyCode::Up;
    case VK_DOWN:
      return KeyCode::Down;
    case VK_LEFT:
      return KeyCode::Left;
    case VK_RIGHT:
      return KeyCode::Right;
    case VK_F1:
      return KeyCode::F1;
    case VK_F2:
      return KeyCode::F2;
    case VK_F3:
      return KeyCode::F3;
    case VK_F4:
      return KeyCode::F4;
    case VK_F5:
      return KeyCode::F5;
    case VK_F6:
      return KeyCode::F6;
    case VK_F7:
      return KeyCode::F7;
    case VK_F8:
      return KeyCode::F8;
    case VK_F9:
      return KeyCode::F9;
    case VK_F10:
      return KeyCode::F10;
    case VK_F11:
      return KeyCode::F11;
    case VK_F12:
      return KeyCode::F12;
    default:
      return KeyCode::Char;
    }
  }

  WinInput::WinInput() {
    in_ = GetStdHandle(STD_INPUT_HANDLE);
    if (in_ == INVALID_HANDLE_VALUE) {
      return;
    }
    GetConsoleMode(in_, &original_mode_);
  }

  WinInput::~WinInput() {
    if (in_ != INVALID_HANDLE_VALUE) {
      SetConsoleMode(in_, original_mode_);
    }
  }

  InputMode WinInput::get_mode() const {
    return mode_;
  }

  void WinInput::set_mode(InputMode mode) {
    if (in_ == INVALID_HANDLE_VALUE)
      return;

    DWORD m = original_mode_;

    // Raw: disable line input and echo, enable VT input.
    if ((mode & InputMode::Raw) != InputMode::None) {
      m &= ~ENABLE_LINE_INPUT;
      m &= ~ENABLE_ECHO_INPUT;
      m |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    }

    // Mouse: enable mouse input.
    if ((mode & InputMode::Mouse) != InputMode::None) {
      m |= ENABLE_MOUSE_INPUT;
    }

    // Paste: no-op in this minimal version (VT bracketed paste is output-side).
    // Keep as a placeholder for future support.

    SetConsoleMode(in_, m);
    mode_ = mode;
  }

  core::Mod WinInput::translate_mods(DWORD state) const noexcept {
    core::Mod mods = core::Mod::None;
    if (state & (SHIFT_PRESSED))
      mods = mods | core::Mod::Shift;
    if (state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))
      mods = mods | core::Mod::Alt;
    if (state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
      mods = mods | core::Mod::Ctrl;
    return mods;
  }

  core::Event WinInput::translate_key(const KEY_EVENT_RECORD &key) {
    if (!key.bKeyDown) {
      return std::monostate{};
    }

    core::KeyEvent ev{};
    ev.repeat = (key.wRepeatCount > 1);
    ev.mods   = translate_mods(key.dwControlKeyState);

    // Handle named keys first (Esc may still have UnicodeChar).
    if (key.wVirtualKeyCode == VK_ESCAPE) {
      ev.code = core::KeyCode::Esc;
      return ev;
    }

    // Treat ASCII ESC (27) as Esc as well.
    if (key.uChar.UnicodeChar == 27) {
      ev.code = core::KeyCode::Esc;
      return ev;
    }

    // Printable character
    if (key.uChar.UnicodeChar != 0) {
      ev.code = core::KeyCode::Char;
      ev.ch   = static_cast<char32_t>(key.uChar.UnicodeChar);
      return ev;
    }

    // Named key
    ev.code = vk_to_keycode(key.wVirtualKeyCode);
    return ev;
  }

  core::Event WinInput::translate_resize(const WINDOW_BUFFER_SIZE_RECORD &sz) {
    core::ResizeEvent ev{};
    ev.size = core::Size{
        static_cast<core::coord_t>(sz.dwSize.X),
        static_cast<core::coord_t>(sz.dwSize.Y)};
    return ev;
  }

  core::Event WinInput::translate_record(const INPUT_RECORD &rec) {
    switch (rec.EventType) {
    case KEY_EVENT:
      return translate_key(rec.Event.KeyEvent);
    case WINDOW_BUFFER_SIZE_EVENT:
      return translate_resize(rec.Event.WindowBufferSizeEvent);
    default:
      return std::monostate{};
    }
  }

  core::Event WinInput::poll() {
    if (in_ == INVALID_HANDLE_VALUE)
      return std::monostate{};

    DWORD count = 0;
    if (!GetNumberOfConsoleInputEvents(in_, &count) || count == 0) {
      return std::monostate{};
    }

    INPUT_RECORD rec{};
    DWORD        read = 0;
    if (!ReadConsoleInputW(in_, &rec, 1, &read) || read == 0) {
      return std::monostate{};
    }

    return translate_record(rec);
  }

  core::Event WinInput::read() {
    if (in_ == INVALID_HANDLE_VALUE)
      return std::monostate{};

    INPUT_RECORD rec{};
    DWORD        read = 0;

    while (true) {
      if (!ReadConsoleInputW(in_, &rec, 1, &read)) {
        return std::monostate{};
      }
      if (read == 0)
        continue;

      auto ev = translate_record(rec);
      if (!std::holds_alternative<std::monostate>(ev)) {
        return ev;
      }
    }
  }

} // namespace glyph::input
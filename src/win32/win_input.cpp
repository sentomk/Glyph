// glyph/input/win_input.cpp
//
// Windows console input backend (minimal).

#include "glyph/input/win32/win_input.h"
#include "glyph/core/event.h"

#include <cassert>
#include <cctype>
#include <cstring>
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
    out_ = GetStdHandle(STD_OUTPUT_HANDLE);
    if (in_ == INVALID_HANDLE_VALUE) {
      return;
    }
    GetConsoleMode(in_, &original_mode_);
  }

  WinInput::~WinInput() {
    if (vt_mouse_enabled_) {
      set_vt_mouse(false);
    }
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

    // Raw: disable line input and echo, enable VT input sequences.
    if ((mode & InputMode::Raw) != InputMode::None) {
      m &= ~ENABLE_LINE_INPUT;
      m &= ~ENABLE_ECHO_INPUT;
      m |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    }

    // Mouse: enable mouse input.
    if ((mode & InputMode::Mouse) != InputMode::None) {
      m |= ENABLE_MOUSE_INPUT;
      m |= ENABLE_WINDOW_INPUT;
      m |= ENABLE_EXTENDED_FLAGS;
      m &= ~ENABLE_QUICK_EDIT_MODE;
    }

    // Paste: no-op in this minimal version (VT bracketed paste is output-side).
    // Keep as a placeholder for future support.

    SetConsoleMode(in_, m);
    mode_ = mode;

    const bool want_mouse = (mode & InputMode::Mouse) != InputMode::None;
    if (want_mouse && !vt_mouse_enabled_) {
      set_vt_mouse(true);
    }
    else if (!want_mouse && vt_mouse_enabled_) {
      set_vt_mouse(false);
    }
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

  void WinInput::emit_key(core::KeyCode code, core::Mod mods, bool repeat) {
    core::KeyEvent ev{};
    ev.code   = code;
    ev.mods   = mods;
    ev.repeat = repeat;
    pending_.push_back(ev);
  }

  void WinInput::emit_char(char32_t ch, core::Mod mods, bool repeat) {
    core::KeyEvent ev{};
    ev.code   = core::KeyCode::Char;
    ev.ch     = ch;
    ev.mods   = mods;
    ev.repeat = repeat;
    pending_.push_back(ev);
  }

  void WinInput::emit_mouse(core::MouseButton button, core::MouseAction action,
                            core::Point pos, core::Mod mods) {
    core::MouseEvent ev{};
    ev.button = button;
    ev.action = action;
    ev.pos    = pos;
    ev.mods   = mods;
    pending_.push_back(ev);
  }

  void WinInput::flush_ansi(bool force) {
    if (!force)
      return;

    if (ansi_state_ == AnsiState::Esc) {
      emit_key(core::KeyCode::Esc, esc_mods_, false);
    }

    ansi_state_ = AnsiState::Ground;
    params_.clear();
    mouse_sgr_ = false;
  }

  void WinInput::process_chars() {
    while (!char_queue_.empty()) {
      const auto in = char_queue_.front();
      char_queue_.pop_front();

      auto handle_ground = [&](char32_t ch, core::Mod mods, bool repeat) {
        if (ch == U'\x1b') {
          ansi_state_ = AnsiState::Esc;
          esc_mods_   = mods;
          return;
        }
        emit_char(ch, mods, repeat);
      };

      switch (ansi_state_) {
      case AnsiState::Ground:
        handle_ground(in.ch, in.mods, false);
        break;
      case AnsiState::Esc:
        if (in.ch == U'[') {
          ansi_state_ = AnsiState::Csi;
          params_.clear();
          break;
        }
        if (in.ch == U'O') {
          ansi_state_ = AnsiState::Ss3;
          break;
        }
        emit_key(core::KeyCode::Esc, esc_mods_, false);
        ansi_state_ = AnsiState::Ground;
        handle_ground(in.ch, in.mods, false);
        break;
      case AnsiState::Ss3:
        switch (in.ch) {
        case U'A':
          emit_key(core::KeyCode::Up, core::Mod::None, false);
          break;
        case U'B':
          emit_key(core::KeyCode::Down, core::Mod::None, false);
          break;
        case U'C':
          emit_key(core::KeyCode::Right, core::Mod::None, false);
          break;
        case U'D':
          emit_key(core::KeyCode::Left, core::Mod::None, false);
          break;
        case U'H':
          emit_key(core::KeyCode::Home, core::Mod::None, false);
          break;
        case U'F':
          emit_key(core::KeyCode::End, core::Mod::None, false);
          break;
        default:
          break;
        }
        ansi_state_ = AnsiState::Ground;
        break;
      case AnsiState::Csi:
        if (mouse_sgr_) {
          if (in.ch == U'M' || in.ch == U'm') {
            int values[3] = {0, 0, 0};
            int idx = 0;
            int current = 0;
            bool has_digit = false;
            for (char32_t ch : params_) {
              if (ch >= U'0' && ch <= U'9') {
                current = current * 10 + int(ch - U'0');
                has_digit = true;
              }
              else if (ch == U';') {
                if (idx < 3) {
                  values[idx++] = has_digit ? current : 0;
                }
                current = 0;
                has_digit = false;
              }
            }
            if (idx < 3) {
              values[idx] = has_digit ? current : 0;
              ++idx;
            }

            if (idx >= 3) {
              const int b = values[0];
              const int x = values[1];
              const int y = values[2];

              core::Mod mods = core::Mod::None;
              if (b & 4)
                mods = mods | core::Mod::Shift;
              if (b & 8)
                mods = mods | core::Mod::Alt;
              if (b & 16)
                mods = mods | core::Mod::Ctrl;

              const core::Point pos{
                  core::coord_t(std::max(0, x - 1)),
                  core::coord_t(std::max(0, y - 1))};

              if (b >= 64 && b <= 65) {
                const auto button = (b == 64) ? core::MouseButton::WheelUp
                                              : core::MouseButton::WheelDown;
                emit_mouse(button, core::MouseAction::Scroll, pos, mods);
              }
              else {
                const int btn = b & 3;
                core::MouseButton button = core::MouseButton::Left;
                if (btn == 1)
                  button = core::MouseButton::Middle;
                else if (btn == 2)
                  button = core::MouseButton::Right;

                if ((b & 32) != 0) {
                  emit_mouse(button, core::MouseAction::Drag, pos, mods);
                }
                else if (in.ch == U'm' || btn == 3) {
                  emit_mouse(button, core::MouseAction::Up, pos, mods);
                }
                else {
                  emit_mouse(button, core::MouseAction::Down, pos, mods);
                }
              }
            }

            ansi_state_ = AnsiState::Ground;
            params_.clear();
            mouse_sgr_ = false;
            break;
          }

          if ((in.ch >= U'0' && in.ch <= U'9') || in.ch == U';') {
            params_.push_back(in.ch);
            break;
          }

          ansi_state_ = AnsiState::Ground;
          params_.clear();
          mouse_sgr_ = false;
          break;
        }

        if (in.ch == U'<') {
          mouse_sgr_ = true;
          params_.clear();
          break;
        }

        if (in.ch == U'~') {
          int param = 0;
          for (char32_t ch : params_) {
            if (!std::isdigit(static_cast<unsigned char>(ch)))
              break;
            param = param * 10 + int(ch - U'0');
          }
          switch (param) {
          case 1:
          case 7:
            emit_key(core::KeyCode::Home, core::Mod::None, false);
            break;
          case 2:
            emit_key(core::KeyCode::Insert, core::Mod::None, false);
            break;
          case 3:
            emit_key(core::KeyCode::Delete, core::Mod::None, false);
            break;
          case 4:
          case 8:
            emit_key(core::KeyCode::End, core::Mod::None, false);
            break;
          case 5:
            emit_key(core::KeyCode::PageUp, core::Mod::None, false);
            break;
          case 6:
            emit_key(core::KeyCode::PageDown, core::Mod::None, false);
            break;
          default:
            break;
          }
          ansi_state_ = AnsiState::Ground;
          break;
        }

        if (in.ch >= U'0' && in.ch <= U'9') {
          params_.push_back(in.ch);
          break;
        }
        if (in.ch == U';') {
          params_.push_back(in.ch);
          break;
        }

        switch (in.ch) {
        case U'A':
          emit_key(core::KeyCode::Up, core::Mod::None, false);
          break;
        case U'B':
          emit_key(core::KeyCode::Down, core::Mod::None, false);
          break;
        case U'C':
          emit_key(core::KeyCode::Right, core::Mod::None, false);
          break;
        case U'D':
          emit_key(core::KeyCode::Left, core::Mod::None, false);
          break;
        case U'H':
          emit_key(core::KeyCode::Home, core::Mod::None, false);
          break;
        case U'F':
          emit_key(core::KeyCode::End, core::Mod::None, false);
          break;
        default:
          break;
        }

        ansi_state_ = AnsiState::Ground;
        params_.clear();
        mouse_sgr_ = false;
        break;
      }
    }
  }

  void WinInput::enqueue_key(const KEY_EVENT_RECORD &key) {
    if (!key.bKeyDown) {
      return;
    }

    const auto mods   = translate_mods(key.dwControlKeyState);
    const bool repeat = (key.wRepeatCount > 1);

    if (key.uChar.UnicodeChar != 0) {
      const auto ch = static_cast<char32_t>(key.uChar.UnicodeChar);
      if (ch == U'\t') {
        emit_key(core::KeyCode::Tab, mods, repeat);
        return;
      }
      if (ch == U'\r' || ch == U'\n') {
        emit_key(core::KeyCode::Enter, mods, repeat);
        return;
      }
      if (ch == U'\b') {
        emit_key(core::KeyCode::Backspace, mods, repeat);
        return;
      }

      char_queue_.push_back(CharInput{ch, mods});
      process_chars();
      return;
    }

    const auto code = vk_to_keycode(key.wVirtualKeyCode);
    if (code != core::KeyCode::Char) {
      emit_key(code, mods, repeat);
    }
  }

  core::Event WinInput::translate_mouse(const MOUSE_EVENT_RECORD &mouse) {
    const auto mods = translate_mods(mouse.dwControlKeyState);
    const core::Point pos{
        static_cast<core::coord_t>(mouse.dwMousePosition.X),
        static_cast<core::coord_t>(mouse.dwMousePosition.Y)};

    if (mouse.dwEventFlags == MOUSE_WHEELED) {
      const auto delta =
          static_cast<SHORT>((mouse.dwButtonState >> 16) & 0xFFFF);
      const auto button = (delta > 0) ? core::MouseButton::WheelUp
                                      : core::MouseButton::WheelDown;
      emit_mouse(button, core::MouseAction::Scroll, pos, mods);
      if (!pending_.empty()) {
        auto ev = pending_.front();
        pending_.pop_front();
        return ev;
      }
      return std::monostate{};
    }

    if (mouse.dwEventFlags == MOUSE_HWHEELED) {
      return std::monostate{};
    }

    if (mouse.dwEventFlags == MOUSE_MOVED) {
      const bool dragging = mouse.dwButtonState != 0;
      core::MouseButton button = core::MouseButton::Left;
      if (mouse.dwButtonState & RIGHTMOST_BUTTON_PRESSED) {
        button = core::MouseButton::Right;
      }
      else if (mouse.dwButtonState & FROM_LEFT_2ND_BUTTON_PRESSED) {
        button = core::MouseButton::Middle;
      }
      emit_mouse(button,
                 dragging ? core::MouseAction::Drag
                          : core::MouseAction::Move,
                 pos, mods);
      if (!pending_.empty()) {
        auto ev = pending_.front();
        pending_.pop_front();
        return ev;
      }
      return std::monostate{};
    }

    DWORD changed = mouse.dwButtonState ^ last_button_state_;
    if (changed == 0) {
      last_button_state_ = mouse.dwButtonState;
      return std::monostate{};
    }

    auto push_button = [&](DWORD mask, core::MouseButton button) {
      if ((changed & mask) == 0) {
        return;
      }
      const bool pressed = (mouse.dwButtonState & mask) != 0;
      emit_mouse(button, pressed ? core::MouseAction::Down
                                 : core::MouseAction::Up,
                 pos, mods);
    };

    push_button(FROM_LEFT_1ST_BUTTON_PRESSED, core::MouseButton::Left);
    push_button(RIGHTMOST_BUTTON_PRESSED, core::MouseButton::Right);
    push_button(FROM_LEFT_2ND_BUTTON_PRESSED, core::MouseButton::Middle);

    last_button_state_ = mouse.dwButtonState;
    if (!pending_.empty()) {
      auto ev = pending_.front();
      pending_.pop_front();
      return ev;
    }
    return std::monostate{};
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
      enqueue_key(rec.Event.KeyEvent);
      return std::monostate{};
    case MOUSE_EVENT:
      return translate_mouse(rec.Event.MouseEvent);
    case WINDOW_BUFFER_SIZE_EVENT:
      return translate_resize(rec.Event.WindowBufferSizeEvent);
    default:
      return std::monostate{};
    }
  }

  core::Event WinInput::poll() {
    if (in_ == INVALID_HANDLE_VALUE)
      return std::monostate{};

    if (!pending_.empty()) {
      auto ev = pending_.front();
      pending_.pop_front();
      return ev;
    }

    DWORD count = 0;
    if (!GetNumberOfConsoleInputEvents(in_, &count) || count == 0) {
      return std::monostate{};
    }

    for (DWORD i = 0; i < count; ++i) {
      INPUT_RECORD rec{};
      DWORD        read = 0;
      if (!ReadConsoleInputW(in_, &rec, 1, &read) || read == 0) {
        break;
      }

      auto ev = translate_record(rec);
      if (!std::holds_alternative<std::monostate>(ev)) {
        return ev;
      }

      if (!pending_.empty()) {
        auto out = pending_.front();
        pending_.pop_front();
        return out;
      }
    }

    DWORD remaining = 0;
    GetNumberOfConsoleInputEvents(in_, &remaining);
    flush_ansi(remaining == 0);

    if (!pending_.empty()) {
      auto out = pending_.front();
      pending_.pop_front();
      return out;
    }

    return std::monostate{};
  }

  core::Event WinInput::read() {
    if (in_ == INVALID_HANDLE_VALUE)
      return std::monostate{};

    INPUT_RECORD rec{};
    DWORD        read = 0;

    while (true) {
      if (!pending_.empty()) {
        auto ev = pending_.front();
        pending_.pop_front();
        return ev;
      }

      if (!ReadConsoleInputW(in_, &rec, 1, &read)) {
        return std::monostate{};
      }
      if (read == 0)
        continue;

      auto ev = translate_record(rec);
      if (!std::holds_alternative<std::monostate>(ev)) {
        return ev;
      }

      if (!pending_.empty()) {
        auto out = pending_.front();
        pending_.pop_front();
        return out;
      }

      DWORD remaining = 0;
      GetNumberOfConsoleInputEvents(in_, &remaining);
      if (remaining == 0) {
        flush_ansi(true);
      }
    }
  }

  void WinInput::set_vt_mouse(bool enabled) {
    if (out_ == INVALID_HANDLE_VALUE) {
      vt_mouse_enabled_ = enabled;
      return;
    }

    const char *seq = enabled ? "\x1b[?1000h\x1b[?1006h"
                              : "\x1b[?1000l\x1b[?1006l";
    DWORD written = 0;
    WriteConsoleA(out_, seq, static_cast<DWORD>(std::strlen(seq)), &written,
                  nullptr);
    vt_mouse_enabled_ = enabled;
  }

} // namespace glyph::input

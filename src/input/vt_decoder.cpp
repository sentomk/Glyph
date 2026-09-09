// glyph/input/detail/vt_decoder.cpp
//
// Shared VT/ANSI escape-sequence decoder implementation.

#include "glyph/input/detail/vt_decoder.h"

#include <algorithm>

namespace glyph::input::detail {

  namespace {

    // Split a parameter run like "1;5" into up to two integer fields.
    // Returns how many fields were present.
    int parse_two(const std::u32string &p, int &a, int &b) {
      a = 0;
      b = 0;
      int cur = 0;
      int n   = 0;
      for (char32_t c : p) {
        if (c == U';') {
          if (n == 0) {
            a = cur;
          } else if (n == 1) {
            b = cur;
          }
          ++n;
          cur = 0;
        } else {
          cur = cur * 10 + int(c - U'0');
        }
      }
      if (n == 0) {
        a = cur;
        return 1;
      }
      if (n == 1) {
        b = cur;
        return 2;
      }
      return n;
    }

    // VT modifier parameter: 1 + (shift 1 | alt 2 | ctrl 4), so 5 =
    // Ctrl, 2 = Shift, 3 = Ctrl+Alt, ... 1 (or absent) = none.
    core::Mod vt_mods(int v) noexcept {
      if (v <= 1) {
        return core::Mod::None;
      }
      core::Mod m = core::Mod::None;
      const int bits = v - 1;
      if (bits & 1)
        m = m | core::Mod::Shift;
      if (bits & 2)
        m = m | core::Mod::Alt;
      if (bits & 4)
        m = m | core::Mod::Ctrl;
      return m;
    }

  } // namespace

  void VtDecoder::emit_char(char32_t ch, core::Mod mods) {
    core::KeyEvent ev{};
    ev.code = core::KeyCode::Char;
    ev.ch   = ch;
    ev.mods = mods;
    pending_.push_back(ev);
  }

  void VtDecoder::emit_key(core::KeyCode code, core::Mod mods) {
    core::KeyEvent ev{};
    ev.code = code;
    ev.mods = mods;
    pending_.push_back(ev);
  }

  void VtDecoder::emit_mouse(core::MouseButton button, core::MouseAction action,
                             core::Point pos, core::Mod mods) {
    core::MouseEvent ev{};
    ev.button = button;
    ev.action = action;
    ev.pos    = pos;
    ev.mods   = mods;
    pending_.push_back(ev);
  }

  core::Event VtDecoder::pop() {
    if (pending_.empty()) {
      return std::monostate{};
    }
    auto ev = pending_.front();
    pending_.pop_front();
    return ev;
  }

  void VtDecoder::handle_ground(char32_t ch, core::Mod mods) {
    if (ch == U'\x1b') {
      state_    = State::Esc;
      esc_mods_ = mods;
      return;
    }

    // C0 control bytes that map to named keys. WinInput intercepts these
    // before feeding us, so this path is exercised by the POSIX backend
    // (and any raw byte stream).
    switch (ch) {
    case U'\r': // CR
    case U'\n': // LF
      emit_key(core::KeyCode::Enter, mods);
      return;
    case U'\t':
      emit_key(core::KeyCode::Tab, mods);
      return;
    case U'\b':   // 0x08
    case 0x7F:    // DEL — what most terminals send for Backspace
      emit_key(core::KeyCode::Backspace, mods);
      return;
    default:
      break;
    }

    // Remaining C0 bytes are Ctrl combos: 0x01..0x1A arrive as
    // Ctrl+'a'..Ctrl+'z' in raw mode, and 0x1C..0x1F as Ctrl+\ ] ^ _.
    // Terminals cannot distinguish Ctrl+letter from the bare byte, so
    // apps see the letter plus Mod::Ctrl instead of a control codepoint.
    if (ch < 0x20) {
      char32_t base = 0;
      switch (ch) {
      case 0x00: base = U' '; break; // Ctrl+@ / Ctrl+Space
      case 0x1C: base = U'\\'; break;
      case 0x1D: base = U']'; break;
      case 0x1E: base = U'^'; break;
      case 0x1F: base = U'_'; break;
      default:   base = char32_t(U'a' + (ch - 1)); break;
      }
      emit_char(base, mods | core::Mod::Ctrl);
      return;
    }

    emit_char(ch, mods);
  }

  void VtDecoder::step_esc(char32_t ch, core::Mod mods) {
    if (ch == U'[') {
      state_    = State::Csi;
      params_.clear();
      return;
    }
    if (ch == U'O') {
      state_ = State::Ss3;
      return;
    }
    // Not an introducer: ESC followed by another key in the same burst
    // is how terminals encode Alt+key. A genuinely lone Esc is resolved
    // by flush() when nothing follows, so this branch means both bytes
    // arrived together. Routing through handle_ground also composes the
    // alt-mod onto named keys (Alt+Enter) and C0 bytes (Alt+Ctrl+x).
    state_ = State::Ground;
    handle_ground(ch, esc_mods_ | core::Mod::Alt);
  }

  void VtDecoder::step_ss3(char32_t ch) {
    switch (ch) {
    case U'A': emit_key(core::KeyCode::Up, core::Mod::None); break;
    case U'B': emit_key(core::KeyCode::Down, core::Mod::None); break;
    case U'C': emit_key(core::KeyCode::Right, core::Mod::None); break;
    case U'D': emit_key(core::KeyCode::Left, core::Mod::None); break;
    case U'H': emit_key(core::KeyCode::Home, core::Mod::None); break;
    case U'F': emit_key(core::KeyCode::End, core::Mod::None); break;
    case U'P': emit_key(core::KeyCode::F1, core::Mod::None); break;
    case U'Q': emit_key(core::KeyCode::F2, core::Mod::None); break;
    case U'R': emit_key(core::KeyCode::F3, core::Mod::None); break;
    case U'S': emit_key(core::KeyCode::F4, core::Mod::None); break;
    default: break;
    }
    state_ = State::Ground;
  }

  void VtDecoder::finish_sgr_mouse(char32_t final_ch) {
    int  values[3] = {0, 0, 0};
    int  idx       = 0;
    int  current   = 0;
    bool has_digit = false;
    for (char32_t ch : params_) {
      if (ch >= U'0' && ch <= U'9') {
        current   = current * 10 + int(ch - U'0');
        has_digit = true;
      }
      else if (ch == U';') {
        if (idx < 3) {
          values[idx++] = has_digit ? current : 0;
        }
        current   = 0;
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

      const core::Point pos{core::coord_t(std::max(0, x - 1)),
                            core::coord_t(std::max(0, y - 1))};

      if (b >= 64 && b <= 65) {
        const auto button = (b == 64) ? core::MouseButton::WheelUp
                                      : core::MouseButton::WheelDown;
        emit_mouse(button, core::MouseAction::Scroll, pos, mods);
      }
      else {
        const int        btn    = b & 3;
        core::MouseButton button = core::MouseButton::Left;
        if (btn == 1)
          button = core::MouseButton::Middle;
        else if (btn == 2)
          button = core::MouseButton::Right;

        if ((b & 32) != 0) {
          emit_mouse(button, core::MouseAction::Drag, pos, mods);
        }
        else if (final_ch == U'm' || btn == 3) {
          emit_mouse(button, core::MouseAction::Up, pos, mods);
        }
        else {
          emit_mouse(button, core::MouseAction::Down, pos, mods);
        }
      }
    }
  }

  void VtDecoder::finish_csi_tilde() {
    int key_param = 0;
    int mod_param = 0;
    const int fields = parse_two(params_, key_param, mod_param);
    const core::Mod mods =
        fields > 1 ? vt_mods(mod_param) : core::Mod::None;
    switch (key_param) {
    case 1:
    case 7: emit_key(core::KeyCode::Home, mods); break;
    case 2: emit_key(core::KeyCode::Insert, mods); break;
    case 3: emit_key(core::KeyCode::Delete, mods); break;
    case 4:
    case 8: emit_key(core::KeyCode::End, mods); break;
    case 5: emit_key(core::KeyCode::PageUp, mods); break;
    case 6: emit_key(core::KeyCode::PageDown, mods); break;
    case 11: emit_key(core::KeyCode::F1, mods); break;
    case 12: emit_key(core::KeyCode::F2, mods); break;
    case 13: emit_key(core::KeyCode::F3, mods); break;
    case 14: emit_key(core::KeyCode::F4, mods); break;
    case 15: emit_key(core::KeyCode::F5, mods); break;
    case 17: emit_key(core::KeyCode::F6, mods); break;
    case 18: emit_key(core::KeyCode::F7, mods); break;
    case 19: emit_key(core::KeyCode::F8, mods); break;
    case 20: emit_key(core::KeyCode::F9, mods); break;
    case 21: emit_key(core::KeyCode::F10, mods); break;
    case 23: emit_key(core::KeyCode::F11, mods); break;
    case 24: emit_key(core::KeyCode::F12, mods); break;
    default: break;
    }
  }

  void VtDecoder::step_csi(char32_t ch) {
    // --- SGR mouse payload (entered after '<') ---
    if (mouse_sgr_) {
      if (ch == U'M' || ch == U'm') {
        finish_sgr_mouse(ch);
        state_ = State::Ground;
        params_.clear();
        mouse_sgr_ = false;
        return;
      }
      if ((ch >= U'0' && ch <= U'9') || ch == U';') {
        params_.push_back(ch);
        return;
      }
      // Malformed: bail out.
      state_ = State::Ground;
      params_.clear();
      mouse_sgr_ = false;
      return;
    }

    if (ch == U'<') {
      mouse_sgr_ = true;
      params_.clear();
      return;
    }

    if (ch == U'~') {
      // Bracketed paste markers: CSI 200 ~ / CSI 201 ~
      if (params_ == U"200") {
        in_paste_ = true;
        paste_buf_.clear();
        state_ = State::Ground;
        params_.clear();
        return;
      }
      if (params_ == U"201") {
        core::PasteEvent ev{};
        ev.text   = paste_buf_;
        in_paste_ = false;
        paste_buf_.clear();
        pending_.push_back(std::move(ev));
        state_ = State::Ground;
        params_.clear();
        return;
      }
      finish_csi_tilde();
      state_ = State::Ground;
      params_.clear();
      return;
    }

    if (ch >= U'0' && ch <= U'9') {
      params_.push_back(ch);
      return;
    }
    if (ch == U';') {
      params_.push_back(ch);
      return;
    }

    // Modifier parameter before the final letter ("1;5C" = Ctrl+Right);
    // the key id field itself is ignored for letter-terminated keys.
    int       key_id   = 0;
    int       mod_param = 0;
    const int fields   = parse_two(params_, key_id, mod_param);
    core::Mod mods     = fields > 1 ? vt_mods(mod_param) : core::Mod::None;

    switch (ch) {
    case U'A': emit_key(core::KeyCode::Up, mods); break;
    case U'B': emit_key(core::KeyCode::Down, mods); break;
    case U'C': emit_key(core::KeyCode::Right, mods); break;
    case U'D': emit_key(core::KeyCode::Left, mods); break;
    case U'H': emit_key(core::KeyCode::Home, mods); break;
    case U'F': emit_key(core::KeyCode::End, mods); break;
    case U'P': emit_key(core::KeyCode::F1, mods); break;
    case U'Q': emit_key(core::KeyCode::F2, mods); break;
    case U'R': emit_key(core::KeyCode::F3, mods); break;
    case U'S': emit_key(core::KeyCode::F4, mods); break;
    default: break;
    }

    state_ = State::Ground;
    params_.clear();
    mouse_sgr_ = false;
  }

  void VtDecoder::feed(char32_t ch, core::Mod base_mods) {
    // While capturing a paste payload, swallow everything until the
    // CSI 201 ~ terminator. We still need to watch for the ESC '[' '2' '0'
    // '1' '~' sequence, so route ESC through the state machine.
    if (in_paste_ && state_ == State::Ground && ch != U'\x1b') {
      paste_buf_.push_back(ch);
      return;
    }

    switch (state_) {
    case State::Ground: handle_ground(ch, base_mods); break;
    case State::Esc:    step_esc(ch, base_mods); break;
    case State::Ss3:    step_ss3(ch); break;
    case State::Csi:    step_csi(ch); break;
    }
  }

  void VtDecoder::flush(bool force) {
    if (!force) {
      return;
    }
    // Only resolve a lone ESC (ESC seen, no introducer yet) into an Esc key.
    // A mid-sequence CSI/SS3/paste must be preserved: it may simply be split
    // across reads, and resetting it here would drop the rest of the
    // sequence (e.g. an arrow key arriving as ESC '[' then 'A').
    if (state_ == State::Esc) {
      emit_key(core::KeyCode::Esc, esc_mods_);
      state_ = State::Ground;
      params_.clear();
      mouse_sgr_ = false;
    }
  }

} // namespace glyph::input::detail

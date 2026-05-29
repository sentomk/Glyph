// glyph/input/detail/vt_decoder.cpp
//
// Shared VT/ANSI escape-sequence decoder implementation.

#include "glyph/input/detail/vt_decoder.h"

#include <algorithm>
#include <cctype>

namespace glyph::input::detail {

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
    // (and any raw byte stream). Other C0 codes (Ctrl+letter) fall through
    // as Char so modifier combos still reach the app.
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

    emit_char(ch, mods);
  }

  void VtDecoder::step_esc(char32_t ch, core::Mod mods) {
    if (ch == U'[') {
      state_ = State::Csi;
      params_.clear();
      return;
    }
    if (ch == U'O') {
      state_ = State::Ss3;
      return;
    }
    // Not an introducer: the ESC was standalone, then reprocess ch.
    emit_key(core::KeyCode::Esc, esc_mods_);
    state_ = State::Ground;
    handle_ground(ch, mods);
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
    int param = 0;
    for (char32_t ch : params_) {
      if (!std::isdigit(static_cast<unsigned char>(ch)))
        break;
      param = param * 10 + int(ch - U'0');
    }
    switch (param) {
    case 1:
    case 7: emit_key(core::KeyCode::Home, core::Mod::None); break;
    case 2: emit_key(core::KeyCode::Insert, core::Mod::None); break;
    case 3: emit_key(core::KeyCode::Delete, core::Mod::None); break;
    case 4:
    case 8: emit_key(core::KeyCode::End, core::Mod::None); break;
    case 5: emit_key(core::KeyCode::PageUp, core::Mod::None); break;
    case 6: emit_key(core::KeyCode::PageDown, core::Mod::None); break;
    case 11: emit_key(core::KeyCode::F1, core::Mod::None); break;
    case 12: emit_key(core::KeyCode::F2, core::Mod::None); break;
    case 13: emit_key(core::KeyCode::F3, core::Mod::None); break;
    case 14: emit_key(core::KeyCode::F4, core::Mod::None); break;
    case 15: emit_key(core::KeyCode::F5, core::Mod::None); break;
    case 17: emit_key(core::KeyCode::F6, core::Mod::None); break;
    case 18: emit_key(core::KeyCode::F7, core::Mod::None); break;
    case 19: emit_key(core::KeyCode::F8, core::Mod::None); break;
    case 20: emit_key(core::KeyCode::F9, core::Mod::None); break;
    case 21: emit_key(core::KeyCode::F10, core::Mod::None); break;
    case 23: emit_key(core::KeyCode::F11, core::Mod::None); break;
    case 24: emit_key(core::KeyCode::F12, core::Mod::None); break;
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

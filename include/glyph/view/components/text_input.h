// glyph/view/components/text_input.h
//
// TextInputView: a single-line, editable text field.
//
// Responsibilities:
//   - Maintain an editable UTF-32 buffer with a caret position.
//   - Translate key events into edits (insert / delete / caret motion).
//   - Scroll horizontally so the caret stays visible in a narrow area.
//   - Render text, caret, and an optional placeholder, clipped to the area.
//
// Behavior notes:
//   - Caret position is measured in codepoints (not display columns).
//   - Wide glyphs (CJK/fullwidth) advance the caret by one codepoint but
//     occupy two columns, using core::cell_width().
//   - The caret cell style is fully user-configurable, so a reverse block,
//     an underline, or any custom look can be selected via set_cursor_cell().

#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

#include "glyph/core/cell.h"
#include "glyph/core/event.h"
#include "glyph/core/geometry.h"
#include "glyph/core/style.h"
#include "glyph/core/text.h"
#include "glyph/view/frame.h"
#include "glyph/view/view.h"

namespace glyph::view {

  class TextInputView final : public View {
  public:
    explicit TextInputView(std::u32string text = U"")
        : text_(std::move(text)) {
      caret_ = static_cast<core::coord_t>(text_.size());
    }

    // -- Content --------------------------------------------------

    TextInputView &set_text(std::u32string text) {
      text_  = std::move(text);
      caret_ = static_cast<core::coord_t>(text_.size());
      clamp_caret();
      return *this;
    }

    [[nodiscard]] const std::u32string &text() const noexcept {
      return text_;
    }

    [[nodiscard]] bool empty() const noexcept {
      return text_.empty();
    }

    void clear() {
      text_.clear();
      caret_  = 0;
      scroll_ = 0;
    }

    // -- Caret ----------------------------------------------------

    [[nodiscard]] core::coord_t caret() const noexcept {
      return caret_;
    }

    void set_caret(core::coord_t pos) {
      caret_ = pos;
      clamp_caret();
    }

    // -- Styling --------------------------------------------------

    // Base cell used for each rendered glyph (carries fg/bg/attrs).
    TextInputView &set_cell(core::Cell cell) {
      cell_ = cell;
      return *this;
    }

    // Cell style applied at the caret column. Defaults to a reverse block
    // (swap of the base cell's fg/bg) computed at render time when unset.
    TextInputView &set_cursor_cell(core::Cell cell) {
      cursor_cell_     = cell;
      has_cursor_cell_ = true;
      return *this;
    }

    // Placeholder shown when the buffer is empty.
    TextInputView &set_placeholder(std::u32string text) {
      placeholder_ = std::move(text);
      return *this;
    }

    TextInputView &set_placeholder_cell(core::Cell cell) {
      placeholder_cell_ = cell;
      return *this;
    }

    // Whether to draw the caret (e.g. hide while not focused or busy).
    TextInputView &set_show_cursor(bool enabled) {
      show_cursor_ = enabled;
      return *this;
    }

    TextInputView &set_focused(bool focused) {
      focused_ = focused;
      return *this;
    }

    // Optional cap on the number of codepoints. <= 0 means unlimited.
    TextInputView &set_max_length(core::coord_t max_len) {
      max_length_ = max_len;
      clamp_caret();
      return *this;
    }

    // -- Editing primitives --------------------------------------

    // Insert a printable codepoint at the caret. Returns false if rejected
    // (control char or max length reached).
    bool insert(char32_t ch) {
      if (ch < U' ') {
        return false; // control characters are not text
      }
      if (max_length_ > 0 &&
          static_cast<core::coord_t>(text_.size()) >= max_length_) {
        return false;
      }
      text_.insert(static_cast<std::size_t>(caret_), 1, ch);
      ++caret_;
      return true;
    }

    // Delete the codepoint before the caret (Backspace).
    bool backspace() {
      if (caret_ <= 0) {
        return false;
      }
      text_.erase(static_cast<std::size_t>(caret_ - 1), 1);
      --caret_;
      return true;
    }

    // Delete the codepoint at the caret (Delete).
    bool del() {
      if (caret_ >= static_cast<core::coord_t>(text_.size())) {
        return false;
      }
      text_.erase(static_cast<std::size_t>(caret_), 1);
      return true;
    }

    void move_left() {
      if (caret_ > 0) {
        --caret_;
      }
    }
    void move_right() {
      if (caret_ < static_cast<core::coord_t>(text_.size())) {
        ++caret_;
      }
    }
    void move_home() {
      caret_ = 0;
    }
    void move_end() {
      caret_ = static_cast<core::coord_t>(text_.size());
    }

    // -- Event handling ------------------------------------------

    // Translate a key event into an edit. Returns true if the event was
    // consumed (caller can then skip its own handling / mark dirty).
    bool handle_key(const core::KeyEvent &key) {
      using core::KeyCode;
      switch (key.code) {
      case KeyCode::Char:
        return insert(key.ch);
      case KeyCode::Backspace:
        return backspace();
      case KeyCode::Delete:
        return del();
      case KeyCode::Left:
        move_left();
        return true;
      case KeyCode::Right:
        move_right();
        return true;
      case KeyCode::Home:
        move_home();
        return true;
      case KeyCode::End:
        move_end();
        return true;
      default:
        return false;
      }
    }

    // -- Rendering -----------------------------------------------

    void render(Frame &f, core::Rect area) const override {
      if (area.empty()) {
        return;
      }

      // Placeholder path: empty buffer, caret at start.
      if (text_.empty() && !placeholder_.empty()) {
        render_placeholder(f, area);
        if (draw_caret()) {
          put_caret(f, area, area.left(), U' ');
        }
        return;
      }

      const core::coord_t area_w = area.size.w;
      const core::coord_t caret_col = column_of(caret_);

      // Adjust horizontal scroll so the caret stays visible.
      core::coord_t scroll = scroll_;
      if (caret_col < scroll) {
        scroll = caret_col;
      } else if (caret_col >= scroll + area_w) {
        scroll = core::coord_t(caret_col - area_w + 1);
      }
      scroll_ = scroll; // cache for stable paging across frames

      // Walk glyphs, emitting those within [scroll, scroll + area_w).
      core::coord_t col = 0;
      const auto    y   = area.top();
      for (char32_t ch : text_) {
        const core::coord_t w = glyph_width(ch);
        if (w <= 0) {
          continue;
        }
        const core::coord_t vis_x = core::coord_t(area.left() + (col - scroll));
        if (col >= scroll && vis_x + w <= area.right()) {
          core::Cell c = cell_;
          c.ch         = ch;
          c.width      = static_cast<std::uint8_t>(w);
          f.set(core::Point{vis_x, y}, c);
        }
        col = core::coord_t(col + w);
      }

      // Caret on top (may sit at end-of-text, on a space).
      if (draw_caret()) {
        const core::coord_t vis_x =
            core::coord_t(area.left() + (caret_col - scroll));
        if (vis_x >= area.left() && vis_x < area.right()) {
          const char32_t under = glyph_at_column(caret_col);
          put_caret(f, area, vis_x, under);
        }
      }
    }

  private:
    // Display column where codepoint index `idx` begins.
    core::coord_t column_of(core::coord_t idx) const noexcept {
      core::coord_t col = 0;
      const auto    n   = std::min<core::coord_t>(
          idx, static_cast<core::coord_t>(text_.size()));
      for (core::coord_t i = 0; i < n; ++i) {
        col = core::coord_t(col + glyph_width(text_[static_cast<std::size_t>(i)]));
      }
      return col;
    }

    // The glyph rendered at a given display column (space if past the end).
    char32_t glyph_at_column(core::coord_t target_col) const noexcept {
      core::coord_t col = 0;
      for (char32_t ch : text_) {
        const core::coord_t w = glyph_width(ch);
        if (w <= 0) {
          continue;
        }
        if (col == target_col) {
          return ch;
        }
        col = core::coord_t(col + w);
      }
      return U' ';
    }

    bool draw_caret() const noexcept {
      return show_cursor_ && focused_;
    }

    void put_caret(Frame &f, core::Rect area, core::coord_t x,
                   char32_t under) const {
      if (x < area.left() || x >= area.right()) {
        return;
      }
      core::Cell c;
      if (has_cursor_cell_) {
        c    = cursor_cell_;
        c.ch = (cursor_cell_.ch != U'\0') ? cursor_cell_.ch : under;
      } else {
        // Default caret: reverse the base cell's fg/bg.
        c          = cell_;
        c.ch       = under;
        c.style.fg_rgb = cell_.style.bg_rgb;
        c.style.bg_rgb = cell_.style.fg_rgb;
        const auto fg_def =
            (cell_.style.flags & core::Style::FlagFgDefault) != 0;
        const auto bg_def =
            (cell_.style.flags & core::Style::FlagBgDefault) != 0;
        c.style.flags = core::Style::FlagFgDefault | core::Style::FlagBgDefault;
        if (!fg_def) {
          c.style.flags = static_cast<std::uint16_t>(
              c.style.flags & ~core::Style::FlagBgDefault);
        }
        if (!bg_def) {
          c.style.flags = static_cast<std::uint16_t>(
              c.style.flags & ~core::Style::FlagFgDefault);
        }
      }
      if (c.width == 0) {
        c.width = 1;
      }
      f.set(core::Point{x, area.top()}, c);
    }

    void render_placeholder(Frame &f, core::Rect area) const {
      core::coord_t x = area.left();
      for (char32_t ch : placeholder_) {
        const core::coord_t w = glyph_width(ch);
        if (w <= 0) {
          continue;
        }
        if (x + w > area.right()) {
          break;
        }
        core::Cell c = placeholder_cell_;
        c.ch         = ch;
        c.width      = static_cast<std::uint8_t>(w);
        f.set(core::Point{x, area.top()}, c);
        x = core::coord_t(x + w);
      }
    }

  private:

    static core::coord_t glyph_width(char32_t ch) noexcept {
      return static_cast<core::coord_t>(core::cell_width(ch));
    }

    void clamp_caret() noexcept {
      const auto n = static_cast<core::coord_t>(text_.size());
      if (caret_ < 0) {
        caret_ = 0;
      } else if (caret_ > n) {
        caret_ = n;
      }
    }

    std::u32string text_{};
    std::u32string placeholder_{};
    core::coord_t  caret_  = 0;
    mutable core::coord_t scroll_ = 0; // leftmost visible column (render cache)
    core::coord_t  max_length_ = 0;

    core::Cell cell_{core::Cell::from_char(U' ')};
    core::Cell cursor_cell_{};
    core::Cell placeholder_cell_{
        core::Cell::from_char(U' ', core::Style{}.dim())};
    bool has_cursor_cell_ = false;
    bool show_cursor_     = true;
    bool focused_         = true;
  };

} // namespace glyph::view

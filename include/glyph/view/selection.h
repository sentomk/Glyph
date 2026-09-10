// glyph/view/selection.h
//
// SelectionModel: track, highlight, and extract a cell-rect selection.
//
// Responsibilities:
//   - Maintain anchor/head points driven by mouse drag or keyboard.
//   - Highlight the selected cells on a rendered frame (reverse video).
//   - Extract the selected text as UTF-8 for the clipboard.
//
// In alt-screen mode the terminal emulator's native selection is
// unreliable (it sees escape output, not the logical layout), so the
// engine manages selection itself.

#pragma once

#include <string>

#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/core/style.h"
#include "glyph/view/frame.h"

namespace glyph::view {

  // ------------------------------------------------------------
  // SelectionModel
  // ------------------------------------------------------------
  class SelectionModel final {

  public:
    SelectionModel() = default;

    // Start a selection at a cell coordinate (mouse press).
    void begin(core::Point anchor) noexcept {
      anchor_ = anchor;
      head_   = anchor;
      active_ = true;
    }

    // Move the selection head (mouse drag, shift+click, arrows).
    void extend(core::Point head) noexcept {
      head_ = head;
    }

    void clear() noexcept {
      active_ = false;
    }

    [[nodiscard]] bool active() const noexcept {
      return active_;
    }

    // Normalized bounds: top-left .. bottom-right of the selection.
    [[nodiscard]] core::Rect rect() const noexcept {
      const core::coord_t x0 = anchor_.x < head_.x ? anchor_.x : head_.x;
      const core::coord_t y0 = anchor_.y < head_.y ? anchor_.y : head_.y;
      const core::coord_t x1 = anchor_.x > head_.x ? anchor_.x : head_.x;
      const core::coord_t y1 = anchor_.y > head_.y ? anchor_.y : head_.y;
      return core::Rect{core::Point{x0, y0},
                        core::Size{x1 - x0 + 1, y1 - y0 + 1}};
    }

    // Apply reverse video to the selected cells of a rendered frame.
    // No-op when inactive or fully outside the frame.
    void highlight(Frame &f) const {
      if (!active_) {
        return;
      }
      const core::Rect r = clamp(f);
      for (core::coord_t y = r.top(); y < r.bottom(); ++y) {
        for (core::coord_t x = r.left(); x < r.right(); ++x) {
          core::Cell c = f.at(x, y);
          if (c.width == 0) {
            continue; // spacer of a wide glyph: nothing to restyle
          }
          c.style.attrs =
              static_cast<std::uint16_t>(c.style.attrs |
                                         core::Style::AttrReverse);
          f.set(core::Point{x, y}, c);
        }
      }
    }

    // Extract the selected text as UTF-8. Wide glyphs emit their single
    // codepoint and skip the paired spacer; each row is trimmed of
    // trailing spaces; rows are joined with '\n' (a single-row
    // selection carries no trailing newline).
    [[nodiscard]] std::string extract(const Frame &f) const {
      std::string out;
      if (!active_) {
        return out;
      }
      const core::Rect r = clamp(f);
      bool             first_row = true;
      for (core::coord_t y = r.top(); y < r.bottom(); ++y) {
        if (!first_row) {
          out.push_back('\n');
        }
        first_row = false;

        std::string row;
        for (core::coord_t x = r.left(); x < r.right(); ++x) {
          const core::Cell c = f.at(x, y);
          if (c.width == 0) {
            continue; // spacer of a wide glyph
          }
          if (c.ch == U' ') {
            row.push_back(' ');
          } else {
            append_utf8(row, c.ch);
          }
        }
        while (!row.empty() && row.back() == ' ') {
          row.pop_back(); // trim trailing spaces
        }
        out += row;
      }
      return out;
    }

  private:
    // Intersection of the selection with the frame bounds.
    [[nodiscard]] core::Rect clamp(const Frame &f) const noexcept {
      const core::Size sz = f.size();
      const core::Rect r  = rect();
      const core::coord_t x0 = r.left() < 0 ? 0 : r.left();
      const core::coord_t y0 = r.top() < 0 ? 0 : r.top();
      const core::coord_t x1 = r.right() > sz.w ? sz.w : r.right();
      const core::coord_t y1 = r.bottom() > sz.h ? sz.h : r.bottom();
      return core::Rect{core::Point{x0, y0},
                        core::Size{x1 - x0, y1 - y0}};
    }

    static void append_utf8(std::string &out, char32_t cp) noexcept {
      if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
      } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
      } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
      } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
      }
    }

    core::Point anchor_{};
    core::Point head_{};
    bool        active_ = false;
  };

} // namespace glyph::view

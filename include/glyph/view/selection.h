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

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

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
    // Flow: terminal-style selection — text between the two points in
    // reading order (first row from the start column to its end, full
    // rows between, last row up to the end column). This is what a
    // drag from mid-word to mid-word should copy.
    // Rect: block selection — the column intersection on every row
    // (vim visual-block style).
    enum class Mode : std::uint8_t { Flow, Rect };

    explicit SelectionModel(Mode mode = Mode::Flow) noexcept
        : mode_(mode) {
    }

    void set_mode(Mode mode) noexcept {
      mode_ = mode;
    }
    [[nodiscard]] Mode mode() const noexcept {
      return mode_;
    }

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
      for_each_span(f, [&](core::coord_t y, core::coord_t x0,
                           core::coord_t x1) {
        for (core::coord_t x = x0; x < x1; ++x) {
          core::Cell c = f.at(x, y);
          if (c.width == 0) {
            continue; // spacer of a wide glyph: nothing to restyle
          }
          c.style.attrs =
              static_cast<std::uint16_t>(c.style.attrs |
                                         core::Style::AttrReverse);
          f.set(core::Point{x, y}, c);
        }
      });
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
      bool first_row = true;
      for_each_span(f, [&](core::coord_t y, core::coord_t x0,
                           core::coord_t x1) {
        if (!first_row) {
          out.push_back('\n');
        }
        first_row = false;

        std::string row;
        for (core::coord_t x = x0; x < x1; ++x) {
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
      });
      return out;
    }

  private:
    // Visit the selected spans of a frame as (row, x_begin, x_end)
    // pairs, clipped to the frame. Flow mode walks reading order;
    // Rect mode visits the column intersection on every row.
    template <typename Fn>
    void for_each_span(const Frame &f, Fn &&fn) const {
      const core::Size sz = f.size();
      if (sz.w <= 0 || sz.h <= 0) {
        return;
      }

      core::Point a = anchor_;
      core::Point b = head_;
      if (b.y < a.y || (b.y == a.y && b.x < a.x)) {
        std::swap(a, b);
      }

      const auto clamp_x = [&](core::coord_t x) {
        return std::clamp(x, core::coord_t{0},
                          core::coord_t(sz.w - 1));
      };
      const auto clamp_y = [&](core::coord_t y) {
        return std::clamp(y, core::coord_t{0},
                          core::coord_t(sz.h - 1));
      };

      if (mode_ == Mode::Rect) {
        const core::coord_t x0 = clamp_x(std::min(a.x, b.x));
        const core::coord_t x1 =
            clamp_x(std::max(a.x, b.x)) + 1;
        for (core::coord_t y = clamp_y(a.y); y <= clamp_y(b.y); ++y) {
          fn(y, x0, x1);
        }
        return;
      }

      // Flow.
      const core::coord_t y0 = clamp_y(a.y);
      const core::coord_t y1 = clamp_y(b.y);
      if (y0 == y1) {
        const core::coord_t x0 = clamp_x(std::min(a.x, b.x));
        const core::coord_t x1 = clamp_x(std::max(a.x, b.x)) + 1;
        fn(y0, x0, x1);
        return;
      }
      // First row: from the start column to the row end; rows between
      // are full; last row: from the row start to the end column.
      fn(y0, clamp_x(a.x), sz.w);
      for (core::coord_t y = y0 + 1; y < y1; ++y) {
        fn(y, 0, sz.w);
      }
      fn(y1, 0, clamp_x(b.x) + 1);
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
    Mode        mode_   = Mode::Flow;
  };

} // namespace glyph::view

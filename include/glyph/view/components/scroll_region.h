// glyph/view/components/scroll_region.h
//
// ScrollRegionView: bounded scrollback of text lines inside a rect.
//
// Responsibilities:
//   - Keep a bounded ring buffer of styled text lines.
//   - Auto-scroll to the bottom as new lines arrive.
//   - Allow scrolling up to inspect older output and back down.
//   - Clip every line to the given area.

#pragma once

#include <cstddef>
#include <deque>
#include <string>
#include <utility>

#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/core/style.h"
#include "glyph/view/frame.h"
#include "glyph/view/text.h"
#include "glyph/view/view.h"

namespace glyph::view {

  // ------------------------------------------------------------
  // ScrollRegionView
  // ------------------------------------------------------------
  class ScrollRegionView : public View {

  public:
    struct Line {
      std::string  text;
      core::Style  style{};
    };

    // max_lines = 0 means unbounded.
    explicit ScrollRegionView(std::size_t max_lines = 1000)
        : max_lines_(max_lines) {
    }

    // Append a line; drops the oldest when the cap is reached and
    // re-follows the bottom.
    void push_line(std::string text, core::Style style = {}) {
      if (max_lines_ > 0 && lines_.size() >= max_lines_) {
        lines_.pop_front();
      }
      lines_.push_back(Line{std::move(text), style});
      offset_ = 0;
    }

    void clear() {
      lines_.clear();
      offset_ = 0;
    }

    // Scroll older output into view; offset counts lines back from the
    // newest. Clamped to the oldest available line at render time.
    void scroll_up(std::size_t count = 1) {
      offset_ += count;
    }

    void scroll_down(std::size_t count = 1) {
      offset_ = offset_ > count ? offset_ - count : 0;
    }

    void scroll_to_bottom() {
      offset_ = 0;
    }

    std::size_t line_count() const {
      return lines_.size();
    }

    // Current offset from the newest line (0 = following the bottom).
    std::size_t scroll_offset() const {
      return offset_;
    }

    // Render the last visible window of lines, newest at the bottom row
    // of the area, shifted up by the scroll offset.
    void render(Frame &f, core::Rect area) const override {
      if (area.empty() || area.size.h <= 0 || lines_.empty()) {
        return;
      }

      const std::size_t total = lines_.size();
      const std::size_t visible =
          static_cast<std::size_t>(area.size.h) < total
              ? static_cast<std::size_t>(area.size.h)
              : total;

      const std::size_t eff_offset =
          total > visible ? std::min(offset_, total - visible) : 0;
      const std::size_t start = total - visible - eff_offset;

      for (std::size_t row = 0; row < visible; ++row) {
        const Line &line = lines_[start + row];
        const auto   clipped =
            clip_to_width(line.text, area.size.w);
        if (clipped.empty()) {
          continue;
        }
        draw_text(f,
                  core::Point{area.left(),
                              core::coord_t(area.top() + row)},
                  clipped, core::Cell::from_char(U' ', line.style));
      }
    }

  private:
    std::deque<Line> lines_;
    std::size_t      max_lines_;
    std::size_t      offset_ = 0;
  };

} // namespace glyph::view

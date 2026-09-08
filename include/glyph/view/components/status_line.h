// glyph/view/components/status_line.h
//
// StatusLineView: single-line inline status composed of styled segments.
//
// Responsibilities:
//   - Render a sequence of (text, style) segments on one row.
//   - Clip the assembled line at the area width.
//
// This is the non-full-screen rendering model: the status line scrolls
// with normal terminal output instead of occupying a fixed position.

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/core/style.h"
#include "glyph/view/frame.h"
#include "glyph/view/text.h"
#include "glyph/view/view.h"

namespace glyph::view {

  // ------------------------------------------------------------
  // StatusLineView
  // ------------------------------------------------------------
  class StatusLineView : public View {

  public:
    struct Segment {
      std::string text;
      core::Style style{};
    };

    StatusLineView() = default;

    explicit StatusLineView(std::vector<Segment> segments)
        : segments_(std::move(segments)) {
    }

    // Replace all segments.
    void set_segments(std::vector<Segment> segments) {
      segments_ = std::move(segments);
    }

    // Append one segment.
    void add_segment(std::string text, core::Style style = {}) {
      segments_.push_back(Segment{std::move(text), style});
    }

    // Render segments left to right on the top row of the area,
    // clipped at the area width.
    void render(Frame &f, core::Rect area) const override {
      if (area.empty() || area.size.w <= 0 || area.size.h <= 0) {
        return;
      }

      core::coord_t x         = area.left();
      core::coord_t remaining = area.size.w;

      for (const Segment &seg : segments_) {
        if (remaining <= 0) {
          break;
        }
        const auto clipped = clip_to_width(seg.text, remaining);
        if (!clipped.empty()) {
          draw_text(f, core::Point{x, area.top()}, clipped,
                    core::Cell::from_char(U' ', seg.style));
          x = core::coord_t(x + text_width(clipped));
        }
        remaining = core::coord_t(remaining - text_width(clipped));
      }
    }

  private:
    std::vector<Segment> segments_;
  };

} // namespace glyph::view

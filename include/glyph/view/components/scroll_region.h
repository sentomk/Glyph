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
#include <vector>

#include "glyph/core/cell.h"
#include "glyph/core/event.h"
#include "glyph/core/geometry.h"
#include "glyph/core/style.h"
#include "glyph/view/frame.h"
#include "glyph/view/text.h"
#include "glyph/view/view.h"

namespace glyph::view {

  // ------------------------------------------------------------
  // ScrollRegionView
  // ------------------------------------------------------------
  // Bounded scrollback of styled text lines. Lines longer than the
  // render width wrap onto additional visual rows (newlines split too),
  // without splitting wide glyphs; the scroll offset counts visual rows.
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

    // Mouse integration: wheel Scroll events scroll the region (issue
    // #5). WheelUp scrolls towards older output, WheelDown back down;
    // wheel_lines() rows per notch (terminals send one event per notch).
    // Drag/press events are ignored — selection handling is app policy.
    void on_mouse(const core::MouseEvent &m) {
      if (m.action != core::MouseAction::Scroll) {
        return;
      }
      if (m.button == core::MouseButton::WheelUp) {
        scroll_up(wheel_lines_);
      } else if (m.button == core::MouseButton::WheelDown) {
        scroll_down(wheel_lines_);
      }
    }

    // Rows scrolled per wheel notch (default 3, like tmux).
    void set_wheel_lines(std::size_t lines) {
      wheel_lines_ = lines == 0 ? 1 : lines;
    }
    std::size_t wheel_lines() const {
      return wheel_lines_;
    }

    std::size_t line_count() const {
      return lines_.size();
    }

    // Current offset from the newest visual row (0 = following the
    // bottom). Counts visual rows, so a wrapped line scrolls by each of
    // its rows.
    std::size_t scroll_offset() const {
      return offset_;
    }

    // Render the last visible window of visual rows, newest at the
    // bottom row of the area, shifted up by the scroll offset.
    void render(Frame &f, core::Rect area) const override {
      if (area.empty() || area.size.h <= 0 || area.size.w <= 0 ||
          lines_.empty()) {
        return;
      }

      std::vector<VisualRow> rows;
      build_rows(rows, area.size.w);
      if (rows.empty()) {
        return;
      }

      const std::size_t total = rows.size();
      const std::size_t visible =
          static_cast<std::size_t>(area.size.h) < total
              ? static_cast<std::size_t>(area.size.h)
              : total;

      const std::size_t eff_offset =
          total > visible ? std::min(offset_, total - visible) : 0;
      const std::size_t start = total - visible - eff_offset;

      for (std::size_t row = 0; row < visible; ++row) {
        const VisualRow &vr      = rows[start + row];
        const auto       clipped = clip_to_width(vr.text, area.size.w);
        if (clipped.empty()) {
          continue;
        }
        draw_text(f,
                  core::Point{area.left(),
                              core::coord_t(area.top() + row)},
                  clipped, core::Cell::from_char(U' ', *vr.style));
      }
    }

  private:
    // A visual row: a slice of a logical line's text plus that line's
    // style. The slices stay valid: deque push at either end keeps
    // references to existing elements, and line text is never mutated
    // after push.
    struct VisualRow {
      std::string_view  text;
      const core::Style *style;
    };

    // Expand the ring into visual rows at the given width: newlines
    // split, long segments wrap without splitting wide glyphs, and an
    // empty segment still occupies one blank row.
    void build_rows(std::vector<VisualRow> &out,
                    core::coord_t          width) const {
      for (const Line &line : lines_) {
        std::string_view rest = line.text;
        for (;;) {
          const std::size_t     nl  = rest.find('\n');
          const std::string_view seg =
              nl == std::string_view::npos ? rest : rest.substr(0, nl);

          if (seg.empty()) {
            out.push_back({std::string_view{}, &line.style});
          } else {
            std::string_view r = seg;
            while (!r.empty()) {
              const std::string_view part = clip_to_width(r, width);
              if (part.empty()) {
                // A zero-width cluster right at the boundary: drop one
                // byte so the walk always makes progress.
                r.remove_prefix(1);
                continue;
              }
              out.push_back({part, &line.style});
              r.remove_prefix(part.size());
            }
          }

          if (nl == std::string_view::npos) {
            break;
          }
          rest = rest.substr(nl + 1);
        }
      }
    }

    std::deque<Line> lines_;
    std::size_t      max_lines_;
    std::size_t      offset_      = 0;
    std::size_t      wheel_lines_ = 3;
  };

} // namespace glyph::view

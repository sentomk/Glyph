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

    // Replace the newest line's text in place (streaming updates:
    // growing text re-wraps only its own rows, so other lines' wrapped
    // positions — and any content-anchored selection — stay stable).
    // Unlike push_line this does NOT re-follow the bottom: mutating
    // existing content must not yank a reader who scrolled up.
    // No-op when the ring is empty.
    void replace_last_line(std::string text, core::Style style = {}) {
      if (lines_.empty()) {
        return;
      }
      lines_.back() = Line{std::move(text), style};
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
    // split, segments word-wrap (break at the last space that fits;
    // words longer than the width hard-break, never splitting a wide
    // glyph), and an empty segment still occupies one blank row.
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
            wrap_segment(out, seg, width, &line.style);
          }

          if (nl == std::string_view::npos) {
            break;
          }
          rest = rest.substr(nl + 1);
        }
      }
    }

    // Greedy word wrap of one segment. Break opportunities are spaces
    // that already fit in the row; a word longer than the width breaks
    // at the width instead. Trailing spaces at a break are dropped.
    static void wrap_segment(std::vector<VisualRow> &out,
                             std::string_view        seg,
                             core::coord_t           width,
                             const core::Style      *style) {
      std::size_t  row_start   = 0;
      std::size_t  cursor      = 0; // bytes accepted into the row
      std::size_t  break_at    = 0; // bytes after the last fitting space
      core::coord_t used       = 0;

      while (cursor < seg.size()) {
        const core::Grapheme g = core::next_grapheme(seg, cursor);
        const core::coord_t  gw = g.width;

        if (g.base == U' ' && used + gw <= width) {
          break_at = g.next;
          used     = core::coord_t(used + gw);
          cursor   = g.next;
          continue;
        }

        if (used + gw <= width) {
          used   = core::coord_t(used + gw);
          cursor = g.next;
          continue;
        }

        // Overflow: prefer breaking after the last space that fit;
        // hard-break when the row has no break opportunity. A row that
        // is still empty takes the glyph anyway (a wide glyph on a
        // width-1 area) so the walk always progresses.
        std::size_t next_start = 0;
        std::size_t row_end    = 0;
        if (break_at > row_start) {
          row_end    = break_at;
          next_start = break_at;
        } else if (cursor > row_start) {
          row_end    = cursor;
          next_start = cursor;
        } else {
          row_end    = g.next;
          next_start = g.next;
        }

        std::string_view row = seg.substr(row_start, row_end - row_start);
        while (!row.empty() && row.back() == ' ') {
          row.remove_suffix(1); // drop spaces at the break
        }
        out.push_back({row, style});

        row_start = next_start;
        cursor    = next_start;
        break_at  = next_start;
        used      = 0;
        // Re-consume the overflowing glyph on the next row unless it
        // was placed by the empty-row branch above.
        if (row_end < g.next && next_start >= g.next) {
          cursor = g.next;
          used   = gw;
        }
      }

      if (cursor > row_start) {
        out.push_back({seg.substr(row_start, cursor - row_start), style});
      }
    }

    std::deque<Line> lines_;
    std::size_t      max_lines_;
    std::size_t      offset_      = 0;
    std::size_t      wheel_lines_ = 3;
  };

} // namespace glyph::view

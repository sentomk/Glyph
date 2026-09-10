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

#include <algorithm>
#include <cstddef>
#include <deque>
#include <optional>
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
        ++lines_dropped_;
      }
      lines_.push_back(Line{std::move(text), style});
      offset_ = 0;
    }

    void clear() {
      lines_.clear();
      offset_ = 0;
      lines_dropped_ = 0;
      sel_active_ = false;
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

    // Rows scrolled per wheel notch (default 5; 3 felt sluggish in
    // real use, macOS trackpads and touch emitters send frequent small
    // events where the extra step reads as natural speed).
    void set_wheel_lines(std::size_t lines) {
      wheel_lines_ = lines == 0 ? 1 : lines;
    }
    std::size_t wheel_lines() const {
      return wheel_lines_;
    }

    // -- Content-anchored selection ---------------------------------
    //
    // The selection is stored against logical lines (absolute line id
    // + byte offset), not frame cells: it survives scrolling, wrap
    // re-flow, and content growth, and extract_selection() reads the
    // ring itself — rows that scrolled out of view are still captured
    // (tmux-grade copy, the piece frame-anchored selection cannot do).
    // Frame positions refer to the area of the last render().

    void select_begin(core::Point frame_pos) {
      sel_anchor_ = point_at(frame_pos);
      sel_head_   = sel_anchor_;
      sel_active_ = true;
    }

    void select_extend(core::Point frame_pos) {
      sel_head_ = point_at(frame_pos);
    }

    void select_clear() {
      sel_active_ = false;
    }

    bool selection_active() const {
      return sel_active_;
    }

    // Selected text as UTF-8, flow semantics over logical lines: from
    // the start point to the end of its line, whole lines between, up
    // to the end point on its line. Lines dropped from the ring are
    // skipped; an empty selection yields "".
    [[nodiscard]] std::string extract_selection() const {
      std::string out;
      if (!sel_active_ || lines_.empty()) {
        return out;
      }
      ContentPoint a = sel_anchor_;
      ContentPoint b = sel_head_;
      if (b.line < a.line ||
          (b.line == a.line && b.byte < a.byte)) {
        std::swap(a, b);
      }
      const std::size_t total_abs = lines_dropped_ + lines_.size();
      const std::size_t lo        = std::max(a.line, lines_dropped_);
      const std::size_t hi        = std::min(b.line, total_abs - 1);

      bool first = true;
      for (std::size_t abs = lo; abs <= hi; ++abs) {
        const std::string &text = lines_[abs - lines_dropped_].text;
        std::size_t        s0 = 0, s1 = text.size();
        if (a.line == b.line) {
          if (abs != a.line) {
            continue;
          }
          s0 = std::min(a.byte, b.byte);
          s1 = std::min(std::max(a.byte, b.byte) + 1, text.size());
        } else if (abs == a.line) {
          s0 = std::min(a.byte, text.size());
        } else if (abs == b.line) {
          s1 = std::min(b.byte + 1, text.size());
        }

        std::string piece = text.substr(s0, s1 > s0 ? s1 - s0 : 0);
        while (!piece.empty() && piece.back() == ' ') {
          piece.pop_back();
        }
        if (!first) {
          out.push_back('\n');
        }
        first = false;
        out += piece;
      }
      return out;
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

      // Remember the window's content mapping so mouse coordinates can
      // be converted to content points (select_begin/extend).
      last_rows_.clear();
      last_top_   = area.top();
      last_left_  = area.left();
      last_w_     = area.size.w;

      for (std::size_t row = 0; row < visible; ++row) {
        const VisualRow &vr      = rows[start + row];
        const auto       clipped = clip_to_width(vr.text, area.size.w);
        last_rows_.push_back(vr);
        last_rows_.back().text = clipped;
        if (clipped.empty()) {
          continue;
        }
        draw_text(f,
                  core::Point{area.left(),
                              core::coord_t(area.top() + row)},
                  clipped, core::Cell::from_char(U' ', *vr.style));
        if (sel_active_) {
          highlight_row(f, area, row, vr, clipped);
        }
      }
    }

  private:
    // A visual row: a slice of a logical line's text plus that line's
    // identity and byte range. The slices stay valid: deque push at
    // either end keeps references to existing elements, and line text
    // is never mutated after push (replace_last_line is the documented
    // exception, used only for the newest line).
    struct VisualRow {
      std::string_view   text;       // drawn slice (spaces trimmed at break)
      std::size_t        abs_line;   // logical line id since construction
      std::size_t        byte_begin; // into that line's text
      std::size_t        byte_end;   // past the row (pre-trim)
      const core::Style *style;
    };

    // A content-space selection point: which logical line, which byte.
    struct ContentPoint {
      std::size_t line = 0;
      std::size_t byte = 0;
    };

    // Byte span [b0, b1) of line `abs` covered by the selection
    // (flow semantics); nullopt when the line is not selected.
    [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>>
    span_for_line(std::size_t abs, std::size_t line_len) const {
      if (!sel_active_) {
        return std::nullopt;
      }
      ContentPoint a = sel_anchor_;
      ContentPoint b = sel_head_;
      if (b.line < a.line ||
          (b.line == a.line && b.byte < a.byte)) {
        std::swap(a, b);
      }
      if (abs < a.line || abs > b.line) {
        return std::nullopt;
      }
      if (a.line == b.line) {
        const std::size_t lo = std::min(a.byte, b.byte);
        const std::size_t hi = std::max(a.byte, b.byte) + 1;
        return std::make_pair(lo, std::min(hi, line_len));
      }
      if (abs == a.line) {
        return std::make_pair(std::min(a.byte, line_len), line_len);
      }
      if (abs == b.line) {
        return std::make_pair(std::size_t{0},
                              std::min(b.byte + 1, line_len));
      }
      return std::make_pair(std::size_t{0}, line_len);
    }

    // Convert a frame point (against the last rendered area) into a
    // content point. Clamps into the mapped window; before the first
    // render there is no mapping and (0,0) is returned.
    [[nodiscard]] ContentPoint point_at(core::Point p) const {
      if (last_rows_.empty()) {
        return {};
      }
      const auto row = std::clamp<core::coord_t>(
          core::coord_t(p.y - last_top_), core::coord_t{0},
          core::coord_t(last_rows_.size() - 1));
      const VisualRow &m = last_rows_[static_cast<std::size_t>(row)];
      if (m.text.empty()) {
        return {m.abs_line, m.byte_begin};
      }
      // Columns are area-relative: the first drawn cell of a row sits
      // at last_left_, not frame column 0.
      const auto col = std::clamp<core::coord_t>(
          core::coord_t(p.x - last_left_), core::coord_t{0},
          core::coord_t(last_w_ - 1));

      std::size_t   off = 0;
      core::coord_t w   = 0;
      while (off < m.text.size()) {
        const core::Grapheme g = core::next_grapheme(m.text, off);
        if (col >= w && col < w + g.width) {
          return {m.abs_line, m.byte_begin + off};
        }
        w   = core::coord_t(w + g.width);
        off = g.next;
      }
      return {m.abs_line, m.byte_begin + m.text.size()};
    }

    // Reverse-video the selected byte span of one visible row.
    void highlight_row(Frame &f, core::Rect area, std::size_t row,
                       const VisualRow &vr,
                       std::string_view drawn) const {
      if (vr.abs_line < lines_dropped_ ||
          vr.abs_line - lines_dropped_ >= lines_.size()) {
        return;
      }
      const std::size_t len =
          lines_[vr.abs_line - lines_dropped_].text.size();
      const auto span = span_for_line(vr.abs_line, len);
      if (!span) {
        return;
      }
      auto b0 = std::max(span->first, vr.byte_begin);
      auto b1 = std::min(span->second, vr.byte_end);
      if (b0 >= b1) {
        return;
      }

      // Columns of the drawn slice covered by [b0, b1).
      std::size_t        off = 0;
      core::coord_t      w   = 0;
      core::coord_t      x0  = -1;
      core::coord_t      x1  = -1;
      while (off < drawn.size()) {
        const core::Grapheme g = core::next_grapheme(drawn, off);
        if (vr.byte_begin + off >= b0 && vr.byte_begin + off < b1) {
          if (x0 < 0) {
            x0 = w;
          }
          x1 = core::coord_t(w + g.width);
        }
        w   = core::coord_t(w + g.width);
        off = g.next;
      }
      if (x0 < 0) {
        return;
      }
      for (core::coord_t x = x0; x < x1; ++x) {
        core::Cell c = f.at(core::coord_t(area.left() + x),
                            core::coord_t(area.top() + row));
        if (c.width == 0) {
          continue;
        }
        c.style.attrs = static_cast<std::uint16_t>(
            c.style.attrs | core::Style::AttrReverse);
        f.set(core::Point{core::coord_t(area.left() + x),
                          core::coord_t(area.top() + row)},
              c);
      }
    }

    // Expand the ring into visual rows at the given width: newlines
    // split, segments word-wrap (break at the last space that fits;
    // words longer than the width hard-break, never splitting a wide
    // glyph), and an empty segment still occupies one blank row.
    void build_rows(std::vector<VisualRow> &out,
                    core::coord_t          width) const {
      for (std::size_t i = 0; i < lines_.size(); ++i) {
        const Line &      line = lines_[i];
        const std::size_t abs  = lines_dropped_ + i;
        std::string_view  rest = line.text;
        std::size_t       base = 0;
        for (;;) {
          const std::size_t     nl  = rest.find('\n');
          const std::string_view seg =
              nl == std::string_view::npos ? rest : rest.substr(0, nl);

          if (seg.empty()) {
            out.push_back(
                {std::string_view{}, abs, base, base, &line.style});
          } else {
            wrap_segment(out, seg, width, abs, base, &line.style);
          }

          if (nl == std::string_view::npos) {
            break;
          }
          base += nl + 1;
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
                             std::size_t             abs_line,
                             std::size_t             byte_base,
                             const core::Style      *style) {
      std::size_t    row_start = 0;
      std::size_t    cursor    = 0; // bytes accepted into the row
      std::size_t    break_at  = 0; // bytes after the last fitting space
      core::coord_t  used      = 0;

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
        out.push_back({row, abs_line, byte_base + row_start,
                       byte_base + row_end, style});

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
        out.push_back({seg.substr(row_start, cursor - row_start), abs_line,
                       byte_base + row_start, byte_base + cursor, style});
      }
    }

    std::deque<Line> lines_;
    std::size_t      max_lines_;
    std::size_t      offset_        = 0;
    std::size_t      wheel_lines_   = 5;
    std::size_t      lines_dropped_ = 0;

    bool         sel_active_ = false;
    ContentPoint sel_anchor_{};
    ContentPoint sel_head_{};

    // Content mapping of the last rendered window (render() is const;
    // these cache what it drew for mouse-coordinate conversion).
    mutable std::vector<VisualRow> last_rows_;
    mutable core::coord_t          last_top_  = -1;
    mutable core::coord_t          last_left_ = 0;
    mutable core::coord_t          last_w_    = 0;
  };

} // namespace glyph::view

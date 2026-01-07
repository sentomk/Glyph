// glyph/view/components/label.h
//
// LabelView: draw a single-line or multi-line text label.
//
// Responsibilities:
//   - Render UTF-32 text into a Rect.
//   - Support alignment, optional auto-wrapping, and ellipsis clipping.
//   - Clip safely to the given area.
//
// Behavior notes:
//   - WrapMode::None treats text as a single logical line.
//   - Ellipsis is applied only when wrapping is disabled.
//   - Width is computed per codepoint using core::cell_width().

#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/core/text.h"
#include "glyph/view/frame.h"
#include "glyph/view/layout/align.h"
#include "glyph/view/view.h"

namespace glyph::view {

  // ------------------------------------------------------------
  // LabelView
  // ------------------------------------------------------------
  class LabelView : public View {

  public:
    // Wrapping policy for multi-line layout.
    enum class WrapMode : std::uint8_t {
      None,
      Char,
      Word,
    };

    explicit LabelView(std::u32string text = U"",
                       core::Cell     cell = core::Cell::from_char(U' '))
        : text_(std::move(text)), cell_(cell) {
    }

    // Update label contents.
    LabelView &set_text(std::u32string text) {
      text_ = std::move(text);
      return *this;
    }

    // Update the base Cell used for each glyph.
    LabelView &set_cell(core::Cell cell) {
      cell_ = cell;
      return *this;
    }

    // Configure horizontal/vertical alignment inside the area.
    LabelView &set_align(layout::AlignH h, layout::AlignV v) {
      align_h_ = h;
      align_v_ = v;
      return *this;
    }

    // Enable simple auto-wrap by available width.
    LabelView &set_wrap(bool enabled) {
      wrap_mode_ = enabled ? WrapMode::Char : WrapMode::None;
      return *this;
    }

    // Select wrap strategy for multi-line rendering.
    LabelView &set_wrap_mode(WrapMode mode) {
      wrap_mode_ = mode;
      return *this;
    }

    // Enable ellipsis for single-line overflow.
    LabelView &set_ellipsis(bool enabled) {
      ellipsis_ = enabled;
      return *this;
    }

    // Render text into the given area with alignment and clipping.
    void render(Frame &f, core::Rect area) const override {
      if (area.empty() || text_.empty()) {
        return;
      }

      const core::coord_t area_w = area.size.w;
      const core::coord_t area_h = area.size.h;
      if (area_w <= 0 || area_h <= 0) {
        return;
      }

      // Split into logical lines (manual breaks + optional wrap).
      const auto lines = build_lines(text_, area_w);
      if (lines.empty()) {
        return;
      }

      // Determine which subset of lines is visible.
      const core::coord_t total_lines =
          static_cast<core::coord_t>(lines.size());
      const core::coord_t used_lines =
          std::min<core::coord_t>(area_h, total_lines);
      if (used_lines <= 0) {
        return;
      }

      std::size_t start_index = 0;
      if (total_lines > used_lines) {
        switch (align_v_) {
        case layout::AlignV::Top:
        case layout::AlignV::Stretch:
          start_index = 0;
          break;
        case layout::AlignV::Center:
          start_index = static_cast<std::size_t>(
              (total_lines - used_lines) / 2);
          break;
        case layout::AlignV::Bottom:
          start_index = static_cast<std::size_t>(total_lines - used_lines);
          break;
        }
      }

      // Align the visible block vertically within the area.
      core::coord_t y = area.top();
      switch (align_v_) {
      case layout::AlignV::Top:
      case layout::AlignV::Stretch:
        y = area.top();
        break;
      case layout::AlignV::Center:
        y = core::coord_t(area.top() + (area_h - used_lines) / 2);
        break;
      case layout::AlignV::Bottom:
        y = core::coord_t(area.bottom() - used_lines);
        break;
      }

      // Render each visible line with horizontal alignment.
      for (core::coord_t row = 0; row < used_lines; ++row) {
        const auto &line = lines[start_index + row];
        core::coord_t line_w = text_width(line);

        core::coord_t x = area.left();
        if (line_w > area_w) {
          x = area.left();
        }
        else {
          switch (align_h_) {
          case layout::AlignH::Left:
          case layout::AlignH::Stretch:
            x = area.left();
            break;
          case layout::AlignH::Center:
            x = core::coord_t(area.left() + (area_w - line_w) / 2);
            break;
          case layout::AlignH::Right:
            x = core::coord_t(area.right() - line_w);
            break;
          }
        }

        render_line(f, area, core::Point{x, core::coord_t(y + row)}, line,
                    line_w);
      }
    }

  private:
    // Sum codepoint display widths.
    static core::coord_t text_width(const std::u32string &text) noexcept {
      core::coord_t w = 0;
      for (char32_t ch : text) {
        w = core::coord_t(w + core::cell_width(ch));
      }
      return w;
    }

    // Append one line, optionally wrapped to max_w.
    static bool is_break_char(char32_t ch) noexcept {
      return ch == U' ' || ch == U'\t';
    }

    static void trim_leading_space(std::u32string &text) {
      std::size_t start = 0;
      while (start < text.size() && is_break_char(text[start])) {
        ++start;
      }
      if (start > 0) {
        text.erase(0, start);
      }
    }

    static void trim_trailing_space(std::u32string &text) {
      std::size_t end = text.size();
      while (end > 0 && is_break_char(text[end - 1])) {
        --end;
      }
      if (end < text.size()) {
        text.erase(end);
      }
    }

    static void append_wrapped_line(std::vector<std::u32string> &out,
                                    const std::u32string       &line,
                                    core::coord_t max_w,
                                    WrapMode      mode) {
      if (mode == WrapMode::None || max_w <= 0) {
        out.push_back(line);
        return;
      }

      if (line.empty()) {
        out.emplace_back();
        return;
      }

      std::u32string current;
      core::coord_t  width = 0;

      if (mode == WrapMode::Char) {
        for (char32_t ch : line) {
          const core::coord_t w = core::coord_t(core::cell_width(ch));
          if (w <= 0) {
            continue;
          }

          if (w > max_w) {
            continue;
          }

          if (width + w > max_w) {
            out.push_back(current);
            current.clear();
            width = 0;
          }

          current.push_back(ch);
          width = core::coord_t(width + w);
        }

        out.push_back(current);
        return;
      }

      std::size_t last_break = std::u32string::npos;

      auto refresh_last_break = [&]() {
        last_break = std::u32string::npos;
        for (std::size_t i = 0; i < current.size(); ++i) {
          if (is_break_char(current[i])) {
            last_break = i + 1;
          }
        }
      };

      auto flush_at_break = [&]() {
        std::u32string head = current.substr(0, last_break);
        trim_trailing_space(head);
        out.push_back(head);

        std::u32string tail = current.substr(last_break);
        trim_leading_space(tail);
        current = std::move(tail);
        width = text_width(current);
        refresh_last_break();
      };

      for (char32_t ch : line) {
        const core::coord_t w = core::coord_t(core::cell_width(ch));
        if (w <= 0) {
          continue;
        }

        if (w > max_w) {
          continue;
        }

        current.push_back(ch);
        width = core::coord_t(width + w);
        if (is_break_char(ch)) {
          last_break = current.size();
        }

        if (width > max_w) {
          if (last_break != std::u32string::npos) {
            flush_at_break();
          }
          else {
            const char32_t overflow = current.back();
            current.pop_back();
            trim_trailing_space(current);
            out.push_back(current);
            current.clear();
            width = 0;

            current.push_back(overflow);
            width = core::coord_t(width + w);
            refresh_last_break();
          }
        }
      }

      out.push_back(current);
    }

    // Split by '\n' and apply wrapping if enabled.
    std::vector<std::u32string> build_lines(const std::u32string &text,
                                            core::coord_t max_w) const {
      std::vector<std::u32string> lines;
      std::u32string              current;

      for (char32_t ch : text) {
        if (ch == U'\n') {
          append_wrapped_line(lines, current, max_w, wrap_mode_);
          current.clear();
          continue;
        }
        current.push_back(ch);
      }

      append_wrapped_line(lines, current, max_w, wrap_mode_);
      return lines;
    }

    // Render a line at origin, applying clipping and optional ellipsis.
    void render_line(Frame &f, core::Rect area, core::Point origin,
                     const std::u32string &line, core::coord_t line_w) const {
      const core::coord_t max_w = area.size.w;
      if (max_w <= 0) {
        return;
      }

      const bool          apply_ellipsis =
          ellipsis_ && wrap_mode_ == WrapMode::None && line_w > max_w;
      const core::coord_t ellipsis_w =
          apply_ellipsis ? std::min<core::coord_t>(3, max_w) : 0;
      const core::coord_t content_w = apply_ellipsis
                                          ? core::coord_t(max_w - ellipsis_w)
                                          : max_w;

      core::coord_t cursor  = origin.x;
      core::coord_t used_w  = 0;
      const auto    y_coord = origin.y;

      for (char32_t ch : line) {
        const core::coord_t w = core::coord_t(core::cell_width(ch));
        if (w <= 0) {
          continue;
        }

        if (used_w + w > content_w) {
          break;
        }

        if (cursor + w <= area.left()) {
          cursor = core::coord_t(cursor + w);
          used_w = core::coord_t(used_w + w);
          continue;
        }

        if (cursor >= area.right()) {
          break;
        }

        if (cursor + w > area.right()) {
          break;
        }

        core::Cell c = cell_;
        c.ch         = ch;
        f.set(core::Point{cursor, y_coord}, c);

        cursor = core::coord_t(cursor + w);
        used_w = core::coord_t(used_w + w);
      }

      if (ellipsis_w > 0) {
        const core::coord_t ex = core::coord_t(origin.x + content_w);
        for (core::coord_t i = 0; i < ellipsis_w; ++i) {
          const core::coord_t x = core::coord_t(ex + i);
          if (x < area.left() || x >= area.right()) {
            continue;
          }
          core::Cell c = cell_;
          c.ch         = U'.';
          f.set(core::Point{x, y_coord}, c);
        }
      }
    }

    std::u32string text_;
    core::Cell     cell_{core::Cell::from_char(U' ')};
    layout::AlignH align_h_ = layout::AlignH::Left;
    layout::AlignV align_v_ = layout::AlignV::Top;
    WrapMode       wrap_mode_ = WrapMode::None;
    bool           ellipsis_ = false;
  };

} // namespace glyph::view

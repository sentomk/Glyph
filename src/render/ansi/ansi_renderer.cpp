// glyph/render/ansi/ansi_renderer.cpp
//
// ANSI renderer with dirty-line diff spans.
//
// Notes:
//   - Full redraw on first frame or size change.
//   - Incremental updates via diff spans on dirty lines.
//   - Styles emitted as SGR only when they change.

#include "glyph/render/ansi/ansi_renderer.h"

#include "glyph/core/diff.h"
#include "glyph/view/frame.h"
#include <ostream>
#include <sstream>

namespace glyph::render {

  AnsiRenderer::AnsiRenderer(std::ostream &out) noexcept : out_(out) {
  }

  // Clear the entire screen.
  static void ansi_clear(std::ostream &out) {
    out << "\x1b[2J";
  }

  // Move the cursor to home (1,1).
  static void ansi_home(std::ostream &out) {
    out << "\x1b[H";
  }

  // Reset all SGR attributes.
  static void ansi_reset(std::ostream &out) {
    out << "\x1b[0m";
  }

  // Move cursor to zero-based row/col.
  static void ansi_move(
      std::ostream &out, glyph::core::coord_t row, glyph::core::coord_t col) {
    out << "\x1b[" << (row + 1) << ";" << (col + 1) << "H";
  }

  // Enable/disable terminal line wrapping.
  static void ansi_wrap(std::ostream &out, bool enable) {
    out << (enable ? "\x1b[?7h" : "\x1b[?7l");
  }

  // Emit SGR for the given style (true-color + attributes).
  static void ansi_apply_style(std::ostream &out, const glyph::core::Style &s) {
    out << "\x1b[0";

    if (s.attrs & glyph::core::Style::AttrBold) {
      out << ";1";
    }
    if (s.attrs & glyph::core::Style::AttrDim) {
      out << ";2";
    }
    if (s.attrs & glyph::core::Style::AttrItalic) {
      out << ";3";
    }
    if (s.attrs & glyph::core::Style::AttrUnderline) {
      out << ";4";
    }
    if (s.attrs & glyph::core::Style::AttrBlink) {
      out << ";5";
    }
    if (s.attrs & glyph::core::Style::AttrStrike) {
      out << ";9";
    }

    if (s.fg_is_default()) {
      out << ";39";
    }
    else {
      const std::uint32_t rgb = s.fg;
      const std::uint32_t r   = (rgb >> 16) & 0xFFu;
      const std::uint32_t g   = (rgb >> 8) & 0xFFu;
      const std::uint32_t b   = rgb & 0xFFu;
      out << ";38;2;" << r << ";" << g << ";" << b;
    }

    if (s.bg_is_default()) {
      out << ";49";
    }
    else {
      const std::uint32_t rgb = s.bg;
      const std::uint32_t r   = (rgb >> 16) & 0xFFu;
      const std::uint32_t g   = (rgb >> 8) & 0xFFu;
      const std::uint32_t b   = rgb & 0xFFu;
      out << ";48;2;" << r << ";" << g << ";" << b;
    }

    out << "m";
  }

  // Convert a cell to printable ASCII fallback (1 codepoint).
  static char to_ascii(const view::Frame::cell_type &c) noexcept {
    return c.ch ? static_cast<char>(c.ch) : ' ';
  }

  // Render a single dirty span with style tracking.
  static void render_span(
      std::ostream                &out,
      glyph::core::ConstBufferView buf,
      glyph::core::DiffSpan        span,
      glyph::core::Style          &current,
      bool                        &has_current) {
    if (span.empty())
      return;

    ansi_move(out, span.y, span.x0);

    for (glyph::core::coord_t x = span.x0; x < span.x1; ++x) {
      const auto &cell = buf.at(x, span.y);

      if (cell.width == 0) {
        out << ' ';
        continue;
      }

      if (!has_current || cell.style != current) {
        ansi_apply_style(out, cell.style);
        current     = cell.style;
        has_current = true;
      }

      out << to_ascii(cell);

      if (cell.width == 2) {
        out << ' ';
        ++x;
      }
    }
  }

  void AnsiRenderer::render(const view::Frame &frame) {
    if (frame.empty()) {
      if (has_prev_) {
        ansi_clear(out_);
        ansi_home(out_);
        ansi_reset(out_);
        has_prev_ = false;
      }
      return;
    }

    const auto size = frame.size();
    const auto cur  = frame.view();

    // First frame or size change: full redraw into a buffer to avoid scroll.
    if (!has_prev_ || prev_.size() != size) {
      std::ostringstream out;
      out << "\x1b[2J" << "\x1b[H" << "\x1b[0m";
      ansi_wrap(out, false);

      glyph::core::Style current{};
      bool               has_current = false;

      for (glyph::core::coord_t y = 0; y < size.h; ++y) {
        for (glyph::core::coord_t x = 0; x < size.w; ++x) {
          const auto &cell = cur.at(x, y);

          if (cell.width == 0) {
            out << ' ';
            continue;
          }

          if (!has_current || cell.style != current) {
            ansi_apply_style(out, cell.style);
            current     = cell.style;
            has_current = true;
          }

          out << to_ascii(cell);

          if (cell.width == 2) {
            out << ' ';
            ++x;
          }
        }
        if (y + 1 < size.h) {
          out << "\r\n";
        }
      }

      ansi_wrap(out, true);
      ansi_reset(out);
      out_ << out.str();

      prev_.resize(size);
      prev_.blit(cur, glyph::core::Point{0, 0});
      has_prev_ = true;
      return;
    }

    // Dirty lines only.
    const auto dirty_lines = frame.take_dirty_lines();
    if (dirty_lines.empty()) {
      return;
    }

    ansi_wrap(out_, false);

    glyph::core::Style current{};
    bool               has_current = false;

    const auto spans =
        glyph::core::diff_spans(prev_.const_view(), cur, dirty_lines);

    for (const auto &span : spans) {
      render_span(out_, cur, span, current, has_current);
    }

    ansi_wrap(out_, true);
    ansi_reset(out_);

    prev_.blit(cur, glyph::core::Point{0, 0});
  }

} // namespace glyph::render

// glyph/render/ansi/ansi_renderer.cpp
//
// ANSI renderer with diff spans.

#include "glyph/render/ansi/ansi_renderer.h"

#include "glyph/core/buffer.h"
#include "glyph/core/diff.h"

#include "glyph/core/types.h"
#include "glyph/view/frame.h"
#include <ostream>

namespace glyph::render {

  AnsiRenderer::AnsiRenderer(std::ostream &out) noexcept : out_(out) {
  }

  static void ansi_clear(std::ostream &out) {
    out << "\x1b[2J"; // clear screen
  }

  static void ansi_home(std::ostream &out) {
    out << "\x1b[H"; // cursor home
  }

  static void ansi_reset(std::ostream &out) {
    out << "\x1b[0m"; // reset attrs
  }

  static void ansi_move(
      std::ostream &out, glyph::core::coord_t row, glyph::core::coord_t col) {
    // ANSI cursor position is 1-based
    out << "\x1b[" << (row + 1) << ";" << (col + 1) << "H";
  }

  static char to_ascii(const view::Frame::cell_type &c) noexcept {
    // Minimal: treat as ASCII. For non-ASCII, caller should handle properly.
    return c.ch ? static_cast<char>(c.ch) : ' ';
  }

  static void render_span(
      std::ostream                &out,
      glyph::core::ConstBufferView buf,
      glyph::core::DiffSpan        span) {
    if (span.empty())
      return;

    ansi_move(out, span.y, span.x0);

    for (glyph::core::coord_t x = span.x0; x < span.x1; ++x) {
      const auto &cell = buf.at(x, span.y);

      if (cell.width == 0) {
        out << ' ';
        continue;
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

    // First frame or size change: full redraw.
    if (!has_prev_ || prev_.size() != size) {
      ansi_clear(out_);
      ansi_home(out_);

      for (glyph::core::coord_t y = 0; y < size.h; ++y) {
        for (glyph::core::coord_t x = 0; x < size.w; ++x) {
          const auto &cell = cur.at(x, y);

          if (cell.width == 0) {
            out_ << ' ';
            continue;
          }

          out_ << to_ascii(cell);

          if (cell.width == 2) {
            out_ << ' ';
            ++x;
          }
        }
        out_ << "\r\n";
      }

      ansi_reset(out_);

      prev_.resize(size);
      prev_.blit(cur, glyph::core::Point{0, 0});
      has_prev_ = true;
      return;
    }

    // Diff spans only.
    const auto spans = glyph::core::diff_spans(prev_.const_view(), cur);
    for (const auto &span : spans) {
      render_span(out_, cur, span);
    }

    // Update previous buffer.
    prev_.blit(cur, glyph::core::Point{0, 0});
  }

} // namespace glyph::render
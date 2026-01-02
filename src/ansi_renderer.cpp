// glyph/render/ansi/ansi_renderer.cpp
//
// Minimal ANSI renderer impl.

#include "glyph/render/ansi/ansi_renderer.h"
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

  static char to_ascii(const view::Frame::cell_type &c) noexcept {
    // Minimal: treat as ASCII. For non-ASCII, caller should handle properly.
    return c.ch ? static_cast<char>(c.ch) : ' ';
  }

  void AnsiRenderer::render(const view::Frame &frame) {
    ansi_clear(out_);
    ansi_home(out_);

    if (frame.empty()) {
      ansi_reset(out_);
      return;
    }

    const auto size = frame.size();
    const auto buf  = frame.view();

    for (glyph::core::coord_t y = 0; y < size.h; ++y) {
      for (glyph::core::coord_t x = 0; x < size.w; ++x) {
        const auto &cell = buf.at(x, y);

        if (cell.width == 0) {
          out_ << ' ';
          continue;
        }

        out_ << to_ascii(cell);

        if (cell.width == 2) {
          // occupy next column visually
          out_ << ' ';
          ++x;
        }
      }
      out_ << "\r\n";
    }

    ansi_reset(out_);
  }

} // namespace glyph::render
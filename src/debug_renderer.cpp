// glyph/render/debug/debug_renderer.cpp
//
// Implementation of the debug render pipeline.

#include "glyph/render/debug/debug_renderer.h"

#include "glyph/view/frame.h"
#include <ostream>

namespace glyph::render {

  DebugRenderer::DebugRenderer(std::ostream &out) noexcept : out_(out) {
  }

  static char to_ascii(const glyph::view::Frame::cell_type &c) noexcept {
    // NOTE:
    //  - This assumes core::Cell has a printable glyph/char-like member.
    //  - If your Cell uses a different representation (rune, string, etc.),
    //    adjust this mapping here only; the pipeline stays the same.
    return c.ch ? static_cast<char>(c.ch) : ' ';
  }

  void DebugRenderer::render(const view::Frame &frame) {
    // ------------------------------------------------------------
    // Stage 1: frame header
    // ------------------------------------------------------------
    out_ << "[render] begin frame\n";

    const auto size = frame.size();
    out_ << "  size: " << size.w << "x" << size.h << "\n";

    const auto bounds = frame.bounds();
    out_ << "  bounds: (" << bounds.origin.x << ", " << bounds.origin.y << ") "
         << bounds.size.w << "x" << bounds.size.h << "\n";

    if (frame.empty()) {
      out_ << "  <empty>\n";
      out_ << "[render] end frame\n";
      return;
    }

    // ------------------------------------------------------------
    // Stage 2: acquire read-only buffer view
    // ------------------------------------------------------------
    const auto buf = frame.view();

    // ------------------------------------------------------------
    // Stage 3: dump surface
    // ------------------------------------------------------------
    out_ << "  surface:\n";
    for (glyph::core::coord_t y = 0; y < size.h; ++y) {
      out_ << "    ";
      for (glyph::core::coord_t x = 0; x < size.w; ++x) {
        const auto &cell = buf.at(x, y);

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
      out_ << "\n";
    }

    out_ << "[render] end frame\n";
  }

} // namespace glyph::render
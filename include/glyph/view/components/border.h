// glyph/view/components/border.h
//
// BorderView: draw a simple border around an area.
//
// Responsibilities:
//   - Draw a single-cell border on the area perimeter.
//   - Provide a basic building block for composition.

#pragma once

#include "glyph/view/frame.h"
#include "glyph/view/view.h"

namespace glyph::view {

  // ------------------------------------------------------------
  // BorderView
  // ------------------------------------------------------------
  class BorderView : public View {

  public:
    // Construct with a border cell.
    explicit BorderView(core::Cell c) : cell_(c) {
    }

    // Draw the border along the perimeter only.
    void render(Frame &f, core::Rect area) const override {
      if (area.empty()) {
        return;
      }

      const core::coord_t x0 = area.left();
      const core::coord_t y0 = area.top();
      const core::coord_t x1 = area.right() - 1;
      const core::coord_t y1 = area.bottom() - 1;

      if (x0 > x1 || y0 > y1) {
        return;
      }

      for (core::coord_t x = x0; x <= x1; ++x) {
        f.set(core::Point{x, y0}, cell_);
        f.set(core::Point{x, y1}, cell_);
      }

      for (core::coord_t y = y0; y <= y1; ++y) {
        f.set(core::Point{x0, y}, cell_);
        f.set(core::Point{x1, y}, cell_);
      }
    }

  private:
    core::Cell cell_{};
  };

} // namespace glyph::view
// glyph/view/components/border.h
//
// BorderView: draw a simple border around an area.
//
// Responsibilities:
//   - Draw a single-cell border on the area perimeter.
//   - Provide a basic building block for composition.

#pragma once

#include "glyph/view/components/panel.h"
#include "glyph/view/frame.h"
#include "glyph/view/view.h"

namespace glyph::view {

  // ------------------------------------------------------------
  // BorderView
  // ------------------------------------------------------------
  class BorderView : public View {

  public:
    // Construct with a border cell.
    explicit BorderView(core::Cell c) {
      panel_.set_border(c);
    }

    // Draw the border along the perimeter only.
    void render(Frame &f, core::Rect area) const override {
      panel_.render(f, area);
    }

  private:
    PanelView panel_{};
  };

} // namespace glyph::view

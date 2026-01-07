// glyph/view/components/fill.h
//
// FillView: fill an area with a constant cell.
//
// Responsibilities:
//   - Validate Frame clipping behavior via fill_rect().
//   - Provide a trivial building block for composition.

#pragma once

#include "glyph/view/components/panel.h"
#include "glyph/view/frame.h"
#include "glyph/view/view.h"

namespace glyph::view {

  // ------------------------------------------------------------
  // FillView
  // ------------------------------------------------------------
  class FillView : public View {

  public:
    // Construct with a fill cell.
    explicit FillView(core::Cell c) {
      panel_.set_fill(c);
    }

    // Fill the given area.
    void render(Frame &f, core::Rect area) const override {
      panel_.render(f, area);
    }

  private:
    PanelView panel_{};
  };
} // namespace glyph::view

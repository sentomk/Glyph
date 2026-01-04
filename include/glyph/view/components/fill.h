// glyph/view/components/fill.h
//
// FillView: fill an area with a constant cell
//
// Responsibilities:
//   - Validate Frame clipping behavior via fill_rect()
//   - Provide a trivial building block for composition

#pragma once

#include "glyph/view/frame.h"
#include "glyph/view/view.h"

namespace glyph::view {

  // ------------------------------------------------------------
  // FillView
  // ------------------------------------------------------------
  class FillView : public View {

  public:
    explicit FillView(core::Cell c) : cell_(c) {
    }

    void render(Frame &f, core::Rect area) const override {
      if (area.empty()) {
        return;
      }
      f.fill_rect(area, cell_);
    }

  private:
    core::Cell cell_{};
  };
} // namespace glyph::view
// glyph/view/components/bar.h
//
// BarView: fill a strip area with an optional child view.
//
// Responsibilities:
//   - Fill the bar background (optional).
//   - Render a child view over the full bar area.

#pragma once

#include "glyph/view/components/panel.h"
#include "glyph/view/frame.h"
#include "glyph/view/view.h"

namespace glyph::view {

  // ------------------------------------------------------------
  // BarView
  // ------------------------------------------------------------
  class BarView final : public View {
  public:
    explicit BarView(const View *child = nullptr,
                     core::Cell fill = core::Cell::from_char(U' '))
        : child_(child) {
      panel_.set_child(child_);
      panel_.set_fill(fill);
    }

    // Set or replace the child view (non-owning).
    void set_child(const View *child) {
      child_ = child;
      panel_.set_child(child_);
    }

    // Enable background fill and set the fill cell.
    void set_fill(core::Cell cell) {
      panel_.set_fill(cell);
    }

    // Toggle fill rendering without changing the cell value.
    void set_draw_fill(bool enabled) {
      panel_.set_draw_fill(enabled);
    }

    void render(Frame &f, core::Rect area) const override {
      panel_.render(f, area);
    }

  private:
    const View *child_ = nullptr;
    PanelView   panel_{};
  };

} // namespace glyph::view

// glyph/view/components/inset.h
//
// InsetView: render a child view inside padded insets.
//
// Responsibilities:
//   - Shrink the render area by insets.
//   - Render a single child within the inner rect.

#pragma once

#include "glyph/view/components/panel.h"
#include "glyph/view/frame.h"
#include "glyph/view/layout/inset.h"
#include "glyph/view/view.h"

namespace glyph::view {

  // ------------------------------------------------------------
  // InsetView
  // ------------------------------------------------------------
  class InsetView : public View {

  public:
    // Construct with optional child and insets.
    explicit InsetView(const View *child = nullptr,
                       layout::Insets insets = {})
        : child_(child) {
      panel_.set_child(child_);
      panel_.set_padding(insets);
    }

    // Set or replace the child view (non-owning).
    void set_child(const View *child) {
      child_ = child;
      panel_.set_child(child_);
    }

    // Set padding (insets) applied to the child area.
    void set_insets(layout::Insets insets) {
      panel_.set_padding(insets);
    }

    // Render the child view inside the inset rect.
    void render(Frame &f, core::Rect area) const override {
      panel_.render(f, area);
    }

    const View    *child_ = nullptr;
    PanelView      panel_{};
  };

} // namespace glyph::view

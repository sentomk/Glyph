// glyph/view/components/inset.h
//
// InsetView: render a child view inside padded insets.
//
// Responsibilities:
//   - Shrink the render area by insets.
//   - Render a single child within the inner rect.

#pragma once

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
        : child_(child), insets_(insets) {
    }

    // Set or replace the child view (non-owning).
    void set_child(const View *child) {
      child_ = child;
    }

    // Set padding (insets) applied to the child area.
    void set_insets(layout::Insets insets) {
      insets_ = insets;
    }

    // Render the child view inside the inset rect.
    void render(Frame &f, core::Rect area) const override {
      if (area.empty() || child_ == nullptr) {
        return;
      }

      const auto inner = layout::inset_rect(area, clamp_insets(insets_));
      if (inner.empty()) {
        return;
      }

      child_->render(f, inner);
    }

  private:
    // Ensure insets are non-negative to avoid inverted rects.
    static layout::Insets clamp_insets(layout::Insets insets) {
      if (insets.left < 0)
        insets.left = 0;
      if (insets.top < 0)
        insets.top = 0;
      if (insets.right < 0)
        insets.right = 0;
      if (insets.bottom < 0)
        insets.bottom = 0;
      return insets;
    }

    const View    *child_ = nullptr;
    layout::Insets insets_{};
  };

} // namespace glyph::view

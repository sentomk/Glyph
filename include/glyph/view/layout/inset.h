// glyph/view/layout/inset.h
//
// Inset (padding) helpers.
//
// Responsibilities:
//   - Shrink a rect by margins on each side.
//   - Provide a single-child layout convenience.

#pragma once

#include "glyph/core/geometry.h"
#include "types.h"

namespace glyph::view::layout {

  // ------------------------------------------------------------
  // Insets
  // ------------------------------------------------------------
  struct Insets final {
    core::coord_t left   = 0;
    core::coord_t top    = 0;
    core::coord_t right  = 0;
    core::coord_t bottom = 0;

    static constexpr Insets all(core::coord_t v) noexcept {
      return Insets{v, v, v, v};
    }

    static constexpr Insets hv(core::coord_t h, core::coord_t v) noexcept {
      return Insets{h, v, h, v};
    }
  };

  // ------------------------------------------------------------
  // Inset a rect by margins
  // ------------------------------------------------------------
  inline core::Rect inset_rect(core::Rect area, Insets in) noexcept {
    const core::coord_t x0 = core::coord_t(area.left() + in.left);
    const core::coord_t y0 = core::coord_t(area.top() + in.top);
    const core::coord_t x1 = core::coord_t(area.right() - in.right);
    const core::coord_t y1 = core::coord_t(area.bottom() - in.bottom);

    const core::coord_t w = core::coord_t(x1 - x0);
    const core::coord_t h = core::coord_t(y1 - y0);

    if (w <= 0 || h <= 0) {
      return core::Rect{core::Point{x0, y0}, core::Size{0, 0}};
    }

    return core::Rect{core::Point{x0, y0}, core::Size{w, h}};
  }

  // ------------------------------------------------------------
  // Single-child layout from inset rect
  // ------------------------------------------------------------
  inline LayoutResult layout_inset(core::Rect area, Insets in) {
    LayoutResult out{};
    if (area.empty()) {
      return out;
    }

    out.rects.push_back(inset_rect(area, in));
    return out;
  }

} // namespace glyph::view::layout

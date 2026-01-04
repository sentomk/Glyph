// glyph/view/layout/types.h
//
// Architecture:
//   - Layout only slices Rects; components render into Frame.
//   - Layout depends on core geometry only.
//
// Layout model types.
//
// Responsibilities:
//   - Define axis and sizing rules for box layout.
//   - Provide a layout result container.
//   - Keep layout independent from rendering.

#pragma once

#include <cstdint>
#include <vector>

#include "glyph/core/geometry.h"

namespace glyph::view::layout {

  // Main axis selection for linear layouts.
  enum class Axis : std::uint8_t {
    Horizontal,
    Vertical,
  };

  // Box item sizing.
  // main >= 0: fixed size on main axis.
  // main < 0 : flex item; size allocated by flex weight.
  struct BoxItem final {
    core::coord_t main = -1;
    core::coord_t flex = 1;
  };

  // Layout results are pure Rect slices in draw order.
  struct LayoutResult final {
    std::vector<core::Rect> rects{};
  };

} // namespace glyph::view::layout

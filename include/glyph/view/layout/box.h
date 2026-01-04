// glyph/view/layout/box.h
//
// Box layout (HBox/VBox) helpers.
//
// Responsibilities:
//   - Split a rect along a main axis with fixed and flex items.
//   - Apply spacing deterministically.
//   - Return child rects without rendering side effects.

#pragma once

#include "glyph/core/geometry.h"
#include "types.h"

#include <algorithm>
#include <span>
namespace glyph::view::layout {

  // ------------------------------------------------------------
  // Axis helpers
  // ------------------------------------------------------------
  // Extract main-axis size from a Size.
  inline core::coord_t main_size(core::Size s, Axis axis) noexcept {
    return (axis == Axis::Horizontal) ? s.w : s.h;
  }

  // Extract cross-axis size from a Size.
  inline core::coord_t cross_size(core::Size s, Axis axis) noexcept {
    return (axis == Axis::Horizontal) ? s.h : s.w;
  }

  // Build a Size from main/cross values based on axis.
  inline core::Size
  make_size(core::coord_t main, core::coord_t cross, Axis axis) noexcept {
    return (axis == Axis::Horizontal) ? core::Size{main, cross}
                                      : core::Size{cross, main};
  }

  // Build a Point from main/cross values based on axis.
  inline core::Point
  make_point(core::coord_t main, core::coord_t cross, Axis axis) noexcept {
    return (axis == Axis::Horizontal) ? core::Point{main, cross}
                                      : core::Point{cross, main};
  }

  // ------------------------------------------------------------
  // Linear layout: HBox/VBox
  // ------------------------------------------------------------
  // Linear layout: fixed + flex items distributed along the main axis.
  inline LayoutResult layout_box(
      Axis                     axis,
      core::Rect               area,
      std::span<const BoxItem> items,
      core::coord_t            spacing = 0) {
    LayoutResult out{};
    if (area.empty() || items.empty())
      return out;

    // Spacing is clamped to a sane non-negative value.
    spacing = std::max<core::coord_t>(0, spacing);

    // ------------------------------------------------------------
    // Stage 1: compute available main-axis space
    // ------------------------------------------------------------
    const core::coord_t total_main = main_size(area.size, axis);

    // Total spacing between items (n-1 gaps.)
    const core::coord_t total_spacing =
        (items.size() > 1)
            ? spacing * static_cast<core::coord_t>(items.size() - 1)
            : 0;

    // Main-axis space available to items after spacing.
    core::coord_t available = total_main - total_spacing;
    if (available < 0) {
      available = 0;
    }

    // ------------------------------------------------------------
    // Stage 2: accumulate fixed and flex weights.
    // ------------------------------------------------------------
    core::coord_t fixed_sum = 0;
    core::coord_t flex_sum  = 0;

    for (const auto &it : items) {
      if (it.main >= 0) {
        fixed_sum += it.main;
      }
      else {
        flex_sum += std::max<core::coord_t>(1, it.flex);
      }
    }

    // Remaining space distributed across flex items.
    core::coord_t remaining = available - fixed_sum;
    if (remaining < 0) {
      remaining = 0;
    }

    out.rects.reserve(items.size());

    // Cursor tracks the current main-axis offset.
    core::coord_t cursor    = 0;
    core::coord_t used_flex = 0;

    // Find the last flex item so we can absorb any remainder there.
    std::size_t last_flex_index = items.size();
    for (std::size_t i = 0; i < items.size(); ++i) {
      if (items[i].main < 0) {
        last_flex_index = i;
      }
    }

    // ------------------------------------------------------------
    // Stage 3: assign size to each item and emit Rects.
    // ------------------------------------------------------------
    for (std::size_t i = 0; i < items.size(); ++i) {
      const auto &it = items[i];

      core::coord_t main = 0;

      if (it.main >= 0) {
        // Fixed-size item.
        main = it.main;
      }
      else {
        // Flex-size item: proportional share of remaining space.
        const core::coord_t flex = std::max<core::coord_t>(1, it.flex);
        if (flex_sum > 0) {
          if (i == last_flex_index) {
            // Assign all leftover space to the last flex item to avoid drift.
            main = remaining - used_flex;
          }
          else {
            main      = (remaining * flex) / flex_sum;
            used_flex = core::coord_t(used_flex + main);
          }
        }
      }

      // Construct child rect at current cursor position.
      const core::Point origin = area.origin + make_point(cursor, 0, axis);
      const core::Size  size =
          make_size(main, cross_size(area.size, axis), axis);

      out.rects.push_back(core::Rect{origin, size});

      // Advance cursor to the next slot.
      cursor = core::coord_t(cursor + main + spacing);
    }
    return out;
  }

} // namespace glyph::view::layout

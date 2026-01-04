// glyph/view/layout/split.h
//
// Split layout helpers.
//
// Responsibilities:
//   - Split a rect into segments by ratios.
//   - Provide a simple two-pane split.

#pragma once

#include <algorithm>
#include <cstddef>
#include <span>

#include "glyph/core/geometry.h"
#include "types.h"

namespace glyph::view::layout {

  // ------------------------------------------------------------
  // Ratio split
  // ------------------------------------------------------------
  struct SplitRatio final {
    std::uint32_t weight = 1;
  };

  // Split by ratios along a main axis.
  inline LayoutResult layout_split_ratio(
      Axis                        axis,
      core::Rect                  area,
      std::span<const SplitRatio> ratios,
      core::coord_t               spacing = 0) {
    LayoutResult out{};
    if (area.empty() || ratios.empty()) {
      return out;
    }

    spacing = std::max<core::coord_t>(0, spacing);

    const core::coord_t total_main =
        (axis == Axis::Horizontal) ? area.size.w : area.size.h;
    const core::coord_t total_spacing =
        (ratios.size() > 1)
            ? spacing * static_cast<core::coord_t>(ratios.size() - 1)
            : 0;

    core::coord_t available = total_main - total_spacing;
    if (available < 0) {
      available = 0;
    }

    std::uint32_t weight_sum = 0;
    for (const auto &r : ratios) {
      weight_sum += (r.weight == 0u) ? 1u : r.weight;
    }

    out.rects.reserve(ratios.size());

    core::coord_t cursor    = 0;
    core::coord_t used_main = 0;

    for (std::size_t i = 0; i < ratios.size(); ++i) {
      const auto         &r = ratios[i];
      const std::uint32_t w = (r.weight == 0u) ? 1u : r.weight;

      core::coord_t main = 0;
      if (weight_sum > 0) {
        if (i + 1 == ratios.size()) {
          main = available - used_main;
        }
        else {
          main = core::coord_t(
              (static_cast<std::uint64_t>(available) * w) / weight_sum);
          used_main = core::coord_t(used_main + main);
        }
      }

      const core::Point origin =
          area.origin + ((axis == Axis::Horizontal) ? core::Point{cursor, 0}
                                                    : core::Point{0, cursor});
      const core::Size size = (axis == Axis::Horizontal)
                                  ? core::Size{main, area.size.h}
                                  : core::Size{area.size.w, main};

      out.rects.push_back(core::Rect{origin, size});

      cursor = core::coord_t(cursor + main + spacing);
    }

    return out;
  }

  // ------------------------------------------------------------
  // Two-pane split (first fixed, second remainder)
  // ------------------------------------------------------------
  inline LayoutResult layout_split_fixed(
      Axis          axis,
      core::Rect    area,
      core::coord_t first_main,
      core::coord_t spacing = 0) {
    LayoutResult out{};
    if (area.empty()) {
      return out;
    }

    spacing = std::max<core::coord_t>(0, spacing);

    const core::coord_t total_main =
        (axis == Axis::Horizontal) ? area.size.w : area.size.h;
    const core::coord_t available =
        std::max<core::coord_t>(0, total_main - spacing);

    core::coord_t a = std::max<core::coord_t>(0, first_main);
    if (a > available) {
      a = available;
    }

    const core::coord_t b = std::max<core::coord_t>(0, available - a);

    const core::Point origin0 = area.origin;
    const core::Size  size0   = (axis == Axis::Horizontal)
                                    ? core::Size{a, area.size.h}
                                    : core::Size{area.size.w, a};

    const core::Point origin1 =
        area.origin + ((axis == Axis::Horizontal)
                           ? core::Point{a + spacing, 0}
                           : core::Point{0, a + spacing});
    const core::Size size1 = (axis == Axis::Horizontal)
                                 ? core::Size{b, area.size.h}
                                 : core::Size{area.size.w, b};

    out.rects.push_back(core::Rect{origin0, size0});
    out.rects.push_back(core::Rect{origin1, size1});
    return out;
  }

} // namespace glyph::view::layout

// glyph/view/layout/align.h
//
// Align helpers.
//
// Responsibilities:
//   - Place a single rect inside a parent area.
//   - Support horizontal and vertical alignment.
//   - Optionally clamp to requested size.

#pragma once

#include "glyph/core/geometry.h"
#include "types.h"

namespace glyph::view::layout {

  // ------------------------------------------------------------
  // Alignment enums
  // ------------------------------------------------------------
  enum class AlignH : std::uint8_t {
    Left,
    Center,
    Right,
    Stretch,
  };

  enum class AlignV : std::uint8_t {
    Top,
    Center,
    Bottom,
    Stretch,
  };

  // ------------------------------------------------------------
  // Align spec
  // ------------------------------------------------------------
  struct AlignSpec final {
    AlignH     h    = AlignH::Left;
    AlignV     v    = AlignV::Top;
    core::Size size = core::Size{0, 0}; // <=0 means "use area size"
  };

  // ------------------------------------------------------------
  // Compute aligned rect
  // ------------------------------------------------------------
  inline core::Rect align_rect(core::Rect area, AlignSpec spec) noexcept {
    if (area.empty()) {
      return core::Rect{area.origin, core::Size{0, 0}};
    }

    core::coord_t w = spec.size.w;
    core::coord_t h = spec.size.h;

    if (spec.h == AlignH::Stretch || w <= 0) {
      w = area.size.w;
    }
    else if (w > area.size.w) {
      w = area.size.w;
    }

    if (spec.v == AlignV::Stretch || h <= 0) {
      h = area.size.h;
    }
    else if (h > area.size.h) {
      h = area.size.h;
    }

    core::coord_t x = area.left();
    core::coord_t y = area.top();

    switch (spec.h) {
    case AlignH::Left:
    case AlignH::Stretch:
      x = area.left();
      break;
    case AlignH::Center:
      x = core::coord_t(area.left() + (area.size.w - w) / 2);
      break;
    case AlignH::Right:
      x = core::coord_t(area.right() - w);
      break;
    }

    switch (spec.v) {
    case AlignV::Top:
    case AlignV::Stretch:
      y = area.top();
      break;
    case AlignV::Center:
      y = core::coord_t(area.top() + (area.size.h - h) / 2);
      break;
    case AlignV::Bottom:
      y = core::coord_t(area.bottom() - h);
      break;
    }

    if (w <= 0 || h <= 0) {
      return core::Rect{core::Point{x, y}, core::Size{0, 0}};
    }

    return core::Rect{core::Point{x, y}, core::Size{w, h}};
  }

  // ------------------------------------------------------------
  // Single-child layout
  // ------------------------------------------------------------
  inline LayoutResult layout_align(core::Rect area, AlignSpec spec) {
    LayoutResult out{};
    if (area.empty()) {
      return out;
    }

    out.rects.push_back(align_rect(area, spec));
    return out;
  }

} // namespace glyph::view::layout

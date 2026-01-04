// glyph/view/layout/stack.h
//
// Stack layout helpers.
//
// Responsibilities:
//   - Return the same rect for all children (overlay).
//   - Keep layout pure and stateless.

#pragma once

#include <cstddef>

#include "glyph/core/geometry.h"
#include "types.h"

namespace glyph::view::layout {

  // ------------------------------------------------------------
  // Stack layout
  // ------------------------------------------------------------
  inline LayoutResult layout_stack(core::Rect area, std::size_t count) {
    LayoutResult out{};
    if (area.empty() || count == 0) {
      return out;
    }

    out.rects.resize(count, area);
    return out;
  }

} // namespace glyph::view::layout

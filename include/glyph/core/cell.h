// glyph/core/cell.h
//
// The smallest drawing unit in the TUI system.
// ch: what to draw
// style: how to draw

#pragma once
#include <cstdint>
#include "style.h"

namespace glyph::core {

  // Minimal "glyph" unit.
  struct Cell final {
    char32_t ch = U' '; // single codepoint placeholder
    Style    style{};

    friend constexpr bool operator==(Cell a, Cell b) noexcept {
      return a.ch == b.ch && a.style == b.style;
    }
    friend constexpr bool operator!=(Cell a, Cell b) noexcept {
      return !(a == b);
    }
  };

} // namespace glyph::core

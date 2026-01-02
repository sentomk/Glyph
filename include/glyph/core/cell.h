// glyph/core/cell.h
//
// The smallest drawing unit in the TUI system.
// ch: what to draw
// style: how to draw

#pragma once

#include <cstdint>

#include "style.h"
#include "text.h"

namespace glyph::core {

  // Minimal "glyph" unit.
  struct Cell final {
    char32_t     ch    = U' '; // single codepoint placeholder
    std::uint8_t width = 1;    // 0/1/2 columns
    Style        style{};

    static constexpr Cell from_char(char32_t c, Style s = {}) noexcept {
      return Cell{c, cell_width(c), s};
    }

    friend constexpr bool operator==(Cell a, Cell b) noexcept {
      return a.ch == b.ch && a.style == b.style;
    }
    friend constexpr bool operator!=(Cell a, Cell b) noexcept {
      return !(a == b);
    }
  };

} // namespace glyph::core

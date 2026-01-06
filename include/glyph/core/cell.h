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
    std::uint8_t _pad0 = 0;
    std::uint16_t _pad1 = 0;
    Style        style{};

    constexpr Cell() = default;

    constexpr Cell(char32_t c, Style s = {}) noexcept
        : ch(c), width(static_cast<std::uint8_t>(cell_width(c))), style(s) {
    }

    static constexpr Cell from_char(char32_t c, Style s = {}) noexcept {
      return Cell(c, s);
    }

    friend constexpr bool operator==(Cell a, Cell b) noexcept {
      return a.ch == b.ch && a.width == b.width && a.style == b.style;
    }
    friend constexpr bool operator!=(Cell a, Cell b) noexcept {
      return !(a == b);
    }
  };

} // namespace glyph::core

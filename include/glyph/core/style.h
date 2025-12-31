// glyph/core/style.h
#pragma once
#include <cstdint>

namespace glyph::core {

  // Very small style model. Expand later, but keep it a plain value type.
  struct Style final {
    std::uint32_t fg    = 0; // backend-defined color id (keep core dumb)
    std::uint32_t bg    = 0;
    std::uint16_t attrs = 0; // bitmask for bold/underline/etc.

    friend constexpr bool operator==(Style a, Style b) noexcept {
      return a.fg == b.fg && a.bg == b.bg && a.attrs == b.attrs;
    }
    friend constexpr bool operator!=(Style a, Style b) noexcept {
      return !(a == b);
    }
  };

} // namespace glyph::core

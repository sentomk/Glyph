// glyph/core/text.h
//
// Text helpers (codepoint-level)
//
// Responsibilities:
//   - Provide a minimal, deterministic width rule for a single Unicode
//     codepoint.
//   - Keep core independent from heavy unicode libraries (ICU, etc).

#pragma once

#include <cstdint>

namespace glyph::core {

  // Return display width for a single codepoint.
  // 0 = non-printing/control, 1 = narrow, 2 = wide (CJK/fullwidth).
  constexpr std::uint8_t cell_width(char32_t c) noexcept {
    if (c == U'\0')
      return 0;
    if (c < 0x20 || c == 0x7F)
      return 0;

    if ((c >= 0x1100 && c <= 0x115F) || (c >= 0x2E80 && c <= 0xA4CF) ||
        (c >= 0xAC00 && c <= 0xD7A3) || (c >= 0xF900 && c <= 0xFAFF) ||
        (c >= 0xFE10 && c <= 0xFE19) || (c >= 0xFE30 && c <= 0xFE6F) ||
        (c >= 0xFF00 && c <= 0xFF60) || (c >= 0xFFE0 && c <= 0xFFE6))
      return 2;

    return 1;
  }

} // namespace glyph::core
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

  // Width policy placeholder for future grapheme support.
  enum class WidthPolicy : std::uint8_t {
    Codepoint = 0,
    Grapheme  = 1, // reserved; not implemented
  };

  // Return display width for a single codepoint.
  // 0 = non-printing/control, 1 = narrow, 2 = wide (CJK/fullwidth/emoji).
  //
  // This is a codepoint-level rule (phase 1). It marks codepoints that are
  // wide on their own. Sequence-level cases — symbols that are narrow by
  // default but become wide with an emoji variation selector (U+FE0F), ZWJ
  // sequences, regional-indicator flag pairs, skin-tone modifiers — need a
  // grapheme-aware pass (reserved WidthPolicy::Grapheme) and are not handled
  // here. Notably 0x2600-0x27BF (misc symbols / dingbats) default to text
  // presentation (width 1) and are intentionally not widened at this level.
  constexpr std::uint8_t cell_width(char32_t c) noexcept {
    if (c == U'\0')
      return 0;
    if (c < 0x20 || c == 0x7F)
      return 0;

    // CJK / fullwidth (Basic Multilingual Plane).
    if ((c >= 0x1100 && c <= 0x115F) || (c >= 0x2E80 && c <= 0xA4CF) ||
        (c >= 0xAC00 && c <= 0xD7A3) || (c >= 0xF900 && c <= 0xFAFF) ||
        (c >= 0xFE10 && c <= 0xFE19) || (c >= 0xFE30 && c <= 0xFE6F) ||
        (c >= 0xFF00 && c <= 0xFF60) || (c >= 0xFFE0 && c <= 0xFFE6))
      return 2;

    // Emoji / pictographs that are wide on their own (Supplementary
    // Multilingual Plane). Coarse ranges covering the blocks users actually
    // type; over-coverage hits only unassigned codepoints, which is benign.
    if (c == 0x1F004 ||                   // mahjong red dragon
        c == 0x1F0CF ||                   // playing card black joker
        (c >= 0x1F1E6 && c <= 0x1F1FF) || // regional indicators (flags)
        (c >= 0x1F200 && c <= 0x1F2FF) || // enclosed ideographic supplement
        (c >= 0x1F300 && c <= 0x1FAFF))   // pictographs / symbols / ext-A
      return 2;

    return 1;
  }

  // TODO: Future extension point.
  // For now, callers should use cell_width(c).
  constexpr std::uint8_t cell_width(char32_t c, WidthPolicy policy) noexcept {
    return (policy == WidthPolicy::Codepoint) ? cell_width(c) : cell_width(c);
  }

} // namespace glyph::core
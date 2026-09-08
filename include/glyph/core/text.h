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
#include <string_view>

namespace glyph::core {

  // Width policy: codepoint-level rule only, or grapheme-aware segmentation
  // for UTF-8 input (next_grapheme).
  enum class WidthPolicy : std::uint8_t {
    Codepoint = 0,
    Grapheme  = 1,
  };

  // Return display width for a single codepoint.
  // 0 = non-printing/control/combining/format, 1 = narrow, 2 = wide
  // (CJK/fullwidth/emoji).
  //
  // This is a codepoint-level rule. Sequence-level cases — symbols that
  // widen with an emoji variation selector (U+FE0F), ZWJ sequences,
  // regional-indicator flag pairs, skin-tone modifiers — are combining or
  // zero-width members here and are handled at the cluster level by
  // next_grapheme(). Notably 0x2600-0x27BF (misc symbols / dingbats)
  // default to text presentation (width 1).
  constexpr std::uint8_t cell_width(char32_t c) noexcept {
    if (c == U'\0')
      return 0;
    if (c < 0x20 || c == 0x7F)
      return 0;

    // Zero-width: combining marks and format controls. They render as part
    // of the preceding base glyph and must not occupy a cell of their own.
    if ((c >= 0x0300 && c <= 0x036F) || // combining diacritical marks
        (c >= 0x1AB0 && c <= 0x1AFF) || // combining diacritical marks ext
        (c >= 0x1DC0 && c <= 0x1DFF) || // combining diacritical marks supp
        (c >= 0x200B && c <= 0x200F) || // ZWSP/ZWNJ/ZWJ/LRM/RLM
        c == 0x2060 ||                  // word joiner
        (c >= 0x20D0 && c <= 0x20FF) || // combining marks for symbols
        c == 0xFEFF ||                  // BOM / zero-width no-break space
        (c >= 0xFE00 && c <= 0xFE0F) || // variation selectors VS1..VS16
        (c >= 0xFE20 && c <= 0xFE2F) || // combining half marks
        (c >= 0x1F3FB && c <= 0x1F3FF)) // emoji skin-tone modifiers
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
    // Excluded: regional indicators (a lone indicator renders as a narrow
    // letter; pairs become one flag at the grapheme level) and skin-tone
    // modifiers (zero-width combining members).
    if (c == 0x1F004 ||                   // mahjong red dragon
        c == 0x1F0CF ||                   // playing card black joker
        (c >= 0x1F200 && c <= 0x1F2FF) || // enclosed ideographic supplement
        (c >= 0x1F300 && c <= 0x1FAFF))   // pictographs / symbols / ext-A
      return 2;

    return 1;
  }

  // One grapheme cluster extracted from UTF-8 input.
  struct Grapheme {
    std::size_t  next  = 0;  // byte offset just past the cluster
    char32_t     base  = 0;  // cluster's first codepoint (what a Cell stores)
    std::uint8_t width = 0;  // columns the emitted base glyph occupies
  };

  // Consume one grapheme cluster from utf8 at byte offset pos.
  // Requires pos < utf8.size(); pos >= size returns .next == pos.
  //
  // Segmentation is UAX #29 when built with GLYPH_USE_TEXERE (default);
  // otherwise every codepoint is its own cluster. Multi-codepoint clusters
  // (ZWJ families, flags, VS16 sequences) are stored as their base glyph
  // plus zero-width members — width reflects what gets emitted, so column
  // accounting matches what terminals actually draw. Invalid bytes decode
  // to U+FFFD and advance one byte.
  Grapheme next_grapheme(std::string_view utf8, std::size_t pos) noexcept;

} // namespace glyph::core
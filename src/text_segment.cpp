// glyph/core/text_segment.cpp
//
// Grapheme-cluster segmentation over UTF-8 input.
//
// UAX #29 segmentation is delegated to texere (GLYPH_USE_TEXERE); the
// texere types stay inside this translation unit so the public core
// headers remain dependency-free.

#include "glyph/core/text.h"

#ifdef GLYPH_USE_TEXERE
#include <texere/string_view.hpp>
#endif

namespace glyph::core {

  namespace {

    // Decode the first UTF-8 codepoint of s. A malformed lead byte yields
    // U+FFFD with length 1 so callers always make progress.
    struct Decoded {
      char32_t    cp;
      std::size_t len;
    };

    constexpr Decoded decode_first(std::string_view s) noexcept {
      const auto b0 = static_cast<unsigned char>(s[0]);
      if (b0 < 0x80)
        return {static_cast<char32_t>(b0), 1};

      char32_t cp = 0;
      int      len = 0;
      if ((b0 & 0xE0) == 0xC0) {
        cp = b0 & 0x1F;
        len = 2;
      } else if ((b0 & 0xF0) == 0xE0) {
        cp = b0 & 0x0F;
        len = 3;
      } else if ((b0 & 0xF8) == 0xF0) {
        cp = b0 & 0x07;
        len = 4;
      } else {
        return {U'\uFFFD', 1};
      }

      // s is at most a full cluster; if continuation bytes were truncated
      // at the end of input, stop at what is available.
      if (static_cast<std::size_t>(len) > s.size())
        return {U'\uFFFD', 1};

      for (int i = 1; i < len; ++i) {
        const auto bi = static_cast<unsigned char>(s[i]);
        if ((bi & 0xC0) != 0x80)
          return {U'\uFFFD', 1};
        cp = (cp << 6) | (bi & 0x3F);
      }
      return {cp, static_cast<std::size_t>(len)};
    }

  } // namespace

  Grapheme next_grapheme(std::string_view utf8, std::size_t pos) noexcept {
    if (pos >= utf8.size())
      return {};

#ifdef GLYPH_USE_TEXERE
    const txt::string_view sv{utf8.substr(pos)};
    const auto             range = sv.graphemes();
    auto                   it    = range.begin();
    if (it == range.end()) {
      // No cluster boundary found (should not happen for non-empty input);
      // fall through to single-codepoint consumption below.
      const Decoded d = decode_first(utf8.substr(pos));
      return {pos + d.len, d.cp, cell_width(d.cp)};
    }
    const auto    g = *it;
    const Decoded d = decode_first(g.utf8());
    return {pos + g.byte_size(), d.cp, cell_width(d.cp)};
#else
    const Decoded d = decode_first(utf8.substr(pos));
    return {pos + d.len, d.cp, cell_width(d.cp)};
#endif
  }

} // namespace glyph::core

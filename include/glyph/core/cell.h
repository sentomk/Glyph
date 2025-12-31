// glyph/core/cell.h
#pragma once
#include <cstdint>
#include "style.h"

namespace glyph::core {

  // Minimal "glyph" unit: keep it simple at first.
  // Later you can replace 'ch' with a grapheme handle/string view.
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

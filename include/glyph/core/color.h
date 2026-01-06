// glyph/core/color.h
//
// Color helpers for Style (0xRRGGBB).

#pragma once

#include <cstdint>

namespace glyph::core {

  struct Color final {
    std::uint32_t value = 0; // 0xRRGGBB

    constexpr Color() = default;
    constexpr Color(std::uint32_t rgb) noexcept : value(rgb) {
    }

    static constexpr Color rgb(std::uint8_t r, std::uint8_t g,
                               std::uint8_t b) noexcept {
      return Color{(std::uint32_t(r) << 16) | (std::uint32_t(g) << 8) |
                   std::uint32_t(b)};
    }
  };

  namespace colors {
    // Base colors.
    inline constexpr Color Black{0x000000};
    inline constexpr Color White{0xFFFFFF};
    inline constexpr Color Red{0xFF0000};
    inline constexpr Color Green{0x00FF00};
    inline constexpr Color Blue{0x0000FF};
    inline constexpr Color Yellow{0xFFFF00};
    inline constexpr Color Magenta{0xFF00FF};
    inline constexpr Color Cyan{0x00FFFF};

    // Common extensions.
    inline constexpr Color Orange{0xFFA500};
    inline constexpr Color Gold{0xFFD700};
    inline constexpr Color Pink{0xFF69B4};
    inline constexpr Color SkyBlue{0x87CEEB};
    inline constexpr Color Lime{0x32CD32};
    inline constexpr Color Brown{0x8B4513};

    // Grays.
    inline constexpr Color Gray{0x808080};
    inline constexpr Color DarkGray{0x404040};
    inline constexpr Color DimGray{0x696969};
    inline constexpr Color LightGray{0xD3D3D3};
    inline constexpr Color Silver{0xC0C0C0};

    // ANSI 0-7 darks.
    inline constexpr Color Maroon{0x800000};
    inline constexpr Color Navy{0x000080};
    inline constexpr Color Olive{0x808000};
    inline constexpr Color Teal{0x008080};
    inline constexpr Color Purple{0x800080};
  } // namespace colors

} // namespace glyph::core

// glyph/core/style.h
//
// Purely visual state.
// Describe what a cell should look like.

#pragma once
#include <cstdint>

namespace glyph::core {

  // Compact style model (12 bytes) with true-color support.
  // fg/bg store 0xRRGGBB. Defaults are indicated by flags.
  struct Style final {
    std::uint32_t fg    = 0; // 0xRRGGBB
    std::uint32_t bg    = 0; // 0xRRGGBB
    std::uint16_t attrs = 0; // bitmask for bold/underline/etc.
    std::uint16_t flags = FlagFgDefault | FlagBgDefault;

    enum : std::uint16_t {
      AttrBold      = 1u << 0,
      AttrDim       = 1u << 1,
      AttrItalic    = 1u << 2,
      AttrUnderline = 1u << 3,
      AttrBlink     = 1u << 4,
      AttrStrike    = 1u << 5,
    };

    enum : std::uint16_t {
      FlagFgDefault = 1u << 0,
      FlagBgDefault = 1u << 1,
    };

    // Pack 8-bit RGB channels into 0xRRGGBB.
    static constexpr std::uint32_t rgb(
        std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
      return (std::uint32_t(r) << 16) | (std::uint32_t(g) << 8) |
             std::uint32_t(b);
    }

    // Create a style with an explicit foreground color.
    // Note: other fields are left at defaults.
    static constexpr Style with_fg(std::uint32_t rgb_value) noexcept {
      Style s{};
      s.fg    = rgb_value;
      s.flags = static_cast<std::uint16_t>(s.flags & ~FlagFgDefault);
      return s;
    }

    // Create a style with an explicit background color.
    // Note: other fields are left at defaults.
    static constexpr Style with_bg(std::uint32_t rgb_value) noexcept {
      Style s{};
      s.bg    = rgb_value;
      s.flags = static_cast<std::uint16_t>(s.flags & ~FlagBgDefault);
      return s;
    }

    // Create a style that uses the terminal default foreground.
    static constexpr Style with_default_fg() noexcept {
      Style s{};
      s.flags = static_cast<std::uint16_t>(s.flags | FlagFgDefault);
      return s;
    }

    // Create a style that uses the terminal default background.
    static constexpr Style with_default_bg() noexcept {
      Style s{};
      s.flags = static_cast<std::uint16_t>(s.flags | FlagBgDefault);
      return s;
    }

    // Query whether the foreground is set to "default".
    constexpr bool fg_is_default() const noexcept {
      return (flags & FlagFgDefault) != 0;
    }

    // Query whether the background is set to "default".
    constexpr bool bg_is_default() const noexcept {
      return (flags & FlagBgDefault) != 0;
    }

    friend constexpr bool operator==(Style a, Style b) noexcept {
      return a.fg == b.fg && a.bg == b.bg && a.attrs == b.attrs &&
             a.flags == b.flags;
    }
    friend constexpr bool operator!=(Style a, Style b) noexcept {
      return !(a == b);
    }
  };

} // namespace glyph::core

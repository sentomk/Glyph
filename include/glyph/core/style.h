// glyph/core/style.h
//
// Purely visual state.
// Describe what a cell should look like.

#pragma once
#include <cstdint>

#include "color.h"

namespace glyph::core {

  // Compact style model (12 bytes) with true-color support.
  // fg/bg store 0xRRGGBB. Defaults are indicated by flags.
  struct Style final {
    std::uint32_t fg_rgb = 0; // 0xRRGGBB
    std::uint32_t bg_rgb = 0; // 0xRRGGBB
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

    // Fluent API: set explicit foreground color.
    constexpr Style &fg(std::uint32_t rgb_value) noexcept {
      fg_rgb = rgb_value;
      flags = static_cast<std::uint16_t>(flags & ~FlagFgDefault);
      return *this;
    }

    constexpr Style &fg(Color color) noexcept {
      return fg(color.value);
    }

    // Fluent API: set explicit background color.
    constexpr Style &bg(std::uint32_t rgb_value) noexcept {
      bg_rgb = rgb_value;
      flags = static_cast<std::uint16_t>(flags & ~FlagBgDefault);
      return *this;
    }

    constexpr Style &bg(Color color) noexcept {
      return bg(color.value);
    }

    // Fluent API: reset to terminal default colors.
    constexpr Style &default_fg() noexcept {
      flags = static_cast<std::uint16_t>(flags | FlagFgDefault);
      return *this;
    }

    constexpr Style &default_bg() noexcept {
      flags = static_cast<std::uint16_t>(flags | FlagBgDefault);
      return *this;
    }

    // Fluent API: attribute toggles.
    constexpr Style &bold() noexcept {
      attrs = static_cast<std::uint16_t>(attrs | AttrBold);
      return *this;
    }
    constexpr Style &dim() noexcept {
      attrs = static_cast<std::uint16_t>(attrs | AttrDim);
      return *this;
    }
    constexpr Style &italic() noexcept {
      attrs = static_cast<std::uint16_t>(attrs | AttrItalic);
      return *this;
    }
    constexpr Style &underline() noexcept {
      attrs = static_cast<std::uint16_t>(attrs | AttrUnderline);
      return *this;
    }
    constexpr Style &blink() noexcept {
      attrs = static_cast<std::uint16_t>(attrs | AttrBlink);
      return *this;
    }
    constexpr Style &strike() noexcept {
      attrs = static_cast<std::uint16_t>(attrs | AttrStrike);
      return *this;
    }

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
      s.fg_rgb = rgb_value;
      s.flags = static_cast<std::uint16_t>(s.flags & ~FlagFgDefault);
      return s;
    }

    static constexpr Style with_fg(Color color) noexcept {
      return with_fg(color.value);
    }

    // Create a style with an explicit background color.
    // Note: other fields are left at defaults.
    static constexpr Style with_bg(std::uint32_t rgb_value) noexcept {
      Style s{};
      s.bg_rgb = rgb_value;
      s.flags = static_cast<std::uint16_t>(s.flags & ~FlagBgDefault);
      return s;
    }

    static constexpr Style with_bg(Color color) noexcept {
      return with_bg(color.value);
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
      return a.fg_rgb == b.fg_rgb && a.bg_rgb == b.bg_rgb &&
             a.attrs == b.attrs &&
             a.flags == b.flags;
    }
    friend constexpr bool operator!=(Style a, Style b) noexcept {
      return !(a == b);
    }
  };

} // namespace glyph::core

// glyph/core/geometry.h
//
// Core geometry primitives used across Glyph.
//
// This header is deliberately "dumb": it models integer geometry only.
// No terminal semantics (cell width, unicode width), no layout policy
// (alignment, padding/margin), no styling. Those belong elsewhere.

#pragma once

#include <algorithm>
#include <cstdint>
namespace glyph::core {

  // Coordinate type.
  // Int32 is plenty for typical terminal size while keeping operations cheap.
  using coord_t = std::int32_t;

  // ------------------------------------------------------------
  // Point: an (x, y) location in integer space.
  // ------------------------------------------------------------
  struct Point final {
    coord_t x = 0;
    coord_t y = 0;

    constexpr Point() noexcept = default;
    constexpr Point(coord_t x_, coord_t y_) noexcept : x(x_), y(y_) {
    }

    [[nodiscard]] friend constexpr bool operator==(Point a, Point b) noexcept {
      return a.x == b.x && a.y == b.y;
    }
    [[nodiscard]] friend constexpr bool operator!=(Point a, Point b) noexcept {
      return !(a == b);
    }
    [[nodiscard]] friend constexpr Point operator+(Point a, Point b) noexcept {
      return {coord_t(a.x + b.x), coord_t(a.y + b.y)};
    }
    [[nodiscard]] friend constexpr Point operator-(Point a, Point b) noexcept {
      return {coord_t(a.x - b.x), coord_t(a.y - b.y)};
    }

    constexpr Point &operator+=(Point d) noexcept {
      x = coord_t(x + d.x);
      y = coord_t(y + d.y);

      return *this;
    }
    constexpr Point &operator-=(Point d) noexcept {
      x = coord_t(x - d.x);
      y = coord_t(y - d.y);

      return *this;
    }
  };

  // ------------------------------------------------------------
  // Size: (w, h) extent in integer space.
  // Invariants:
  //  - w, h are non-negative for "well-formed" sizes
  //  - negative sizes can exist transiently but should be normalized by callers
  // ------------------------------------------------------------
  struct Size final {
    coord_t w = 0;
    coord_t h = 0;

    constexpr Size() noexcept = default;
    constexpr Size(coord_t w_, coord_t h_) noexcept : w(w_), h(h_) {
    }

    [[nodiscard]] constexpr bool empty() const noexcept {
      return w <= 0 || h <= 0;
    }

    // Clamp to a non-negative size. Useful as a defensive boundary after
    // arithmetic.
    [[nodiscard]] constexpr Size non_negative() const noexcept {
      return {std::max<coord_t>(0, w), std::max<coord_t>(0, h)};
    }

    [[nodiscard]] friend constexpr bool operator==(Size a, Size b) noexcept {
      return a.w == b.w && a.h == b.h;
    }
    [[nodiscard]] friend constexpr bool operator!=(Size a, Size b) noexcept {
      return !(a == b);
    }
  };

  // ------------------------------------------------------------
  // TODO
  // Rect: origin + size, half-open interval.
  // For well-formed rectangles: w >= 0 and h >= 0.
  // A rect with w==0 or h==0 is valid and represents an empty area.
  // ------------------------------------------------------------

} // namespace glyph::core
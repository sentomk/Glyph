// glyph/core/geometry.h
//
// Core geometry primitives used across Glyph.
//
// This header is deliberately "dumb": it models integer geometry only.
// No terminal semantics (cell width, unicode width), no layout policy
// (alignment, padding/margin), no styling. Those belong elsewhere.

#pragma once

#include <algorithm>

#include <type_traits>

#include "types.h"

namespace glyph::core {

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
  // Rect: origin + size, half-open interval.
  // For well-formed rectangles: w >= 0 and h >= 0.
  // A rect with w==0 or h==0 is valid and represents an empty area.
  // ------------------------------------------------------------
  struct Rect final {
    Point origin{};
    Size  size{};

    constexpr Rect() noexcept = default;
    constexpr Rect(Point o, Size s) noexcept : origin(o), size(s) {
    }
    constexpr Rect(coord_t x, coord_t y, coord_t w, coord_t h) noexcept
        : origin{x, y}, size{w, h} {
    }

    [[nodiscard]] constexpr coord_t left() const noexcept {
      return origin.x;
    }
    [[nodiscard]] constexpr coord_t top() const noexcept {
      return origin.y;
    }
    [[nodiscard]] constexpr coord_t width() const noexcept {
      return size.w;
    }
    [[nodiscard]] constexpr coord_t height() const noexcept {
      return size.h;
    }

    // Half-open: right = left + w, bottom = top + h
    [[nodiscard]] constexpr coord_t right() const noexcept {
      return coord_t(origin.x + size.w);
    }
    [[nodiscard]] constexpr coord_t bottom() const noexcept {
      return coord_t(origin.y + size.h);
    }

    [[nodiscard]] constexpr bool empty() const noexcept {
      return size.empty();
    }

    // Whether (x, y) lies inside this rect using half-open semantics.
    [[nodiscard]] constexpr bool contains(Point p) const noexcept {
      // Treat negative widths/heights as empty.
      if (size.w <= 0 || size.h <= 0) {
        return false;
      }

      return (p.x >= left()) && (p.x < right()) && (p.y >= top()) &&
             (p.y < bottom());
    }

    // Translate by delta.
    [[nodiscard]] constexpr Rect translated(Point d) const noexcept {
      return {origin + d, size};
    }
    constexpr Rect &translate_inplace(Point d) noexcept {
      origin += d;
      return *this;
    }

    // Intersection between two rects. Result may be empty.
    [[nodiscard]] constexpr Rect intersect(Rect other) const noexcept {
      // Normalize "empty if negative" quickly.
      if (size.w <= 0 || size.h <= 0 || other.size.w <= 0 || other.size.h <= 0)
        return Rect{origin, Size{0, 0}};

      const coord_t nx0 = std::max(left(), other.left());
      const coord_t ny0 = std::max(top(), other.top());
      const coord_t nx1 = std::min(right(), other.right());
      const coord_t ny1 = std::min(bottom(), other.bottom());

      const coord_t nw = coord_t(nx1 - nx0);
      const coord_t nh = coord_t(ny1 - ny0);

      if (nw <= 0 || nh <= 0)
        return Rect{Point{nx0, ny0}, Size{0, 0}};

      return Rect{Point{nx0, ny0}, Size{nw, nh}};
    }

    // Bounding union (smallest rect containing both). Handles empties sensibly.
    [[nodiscard]] constexpr Rect unite(Rect other) const noexcept {
      const bool a_empty = (size.w <= 0 || size.h <= 0);
      const bool b_empty = (other.size.w <= 0 || other.size.h <= 0);

      if (a_empty)
        return other;
      if (b_empty)
        return *this;

      const coord_t nx0 = std::min(left(), other.left());
      const coord_t ny0 = std::min(top(), other.top());
      const coord_t nx1 = std::max(right(), other.right());
      const coord_t ny1 = std::max(bottom(), other.bottom());

      return Rect{
          Point{nx0, ny0}, Size{coord_t(nx1 - nx0), coord_t(ny1 - ny0)}};
    }

    // Clip this rect to the given bounds (i.e., intersection with bounds).
    [[nodiscard]] constexpr Rect clipped(Rect bounds) const noexcept {
      return intersect(bounds);
    }

    [[nodiscard]] friend constexpr bool operator==(Rect a, Rect b) noexcept {
      return a.origin == b.origin && a.size == b.size;
    }
    [[nodiscard]] friend constexpr bool operator!=(Rect a, Rect b) noexcept {
      return !(a == b);
    }
  };

  // ------------------------------------------------------------
  // Compile-time sanity checks for ABI assumptions.
  // ------------------------------------------------------------
  static_assert(std::is_trivially_copyable_v<Point>);
  static_assert(std::is_trivially_copyable_v<Size>);
  static_assert(std::is_trivially_copyable_v<Rect>);

  static_assert(sizeof(Point) == sizeof(coord_t) * 2);
  static_assert(sizeof(Size) == sizeof(coord_t) * 2);
  static_assert(sizeof(Rect) == sizeof(coord_t) * 4);

} // namespace glyph::core
// glyph/core/buffer.h
//
// 2D cell buffer (the "canvas")
//
// Responsibilities:
//   - Own or view a rectangular 2D grid of Cells
//   - Provide safe read/write primitives
//   - Provide clipping against bounds

#pragma once

#include "cell.h"
#include "geometry.h"
#include "types.h"

#include <cstddef>
#include <cassert>

namespace glyph::core {

  // ------------------------------------------------------------
  // BufferView: non-owning view over a (sub)rectangle of a buffer-like storage.
  // Row-major contiguous storage assumed, with a configurable row stride.
  // ------------------------------------------------------------
  class BufferView final {
  public:
    using value_type = Cell;

    constexpr BufferView() noexcept = default;

    // Create a view.
    // - data: pointer to first cell of the view (top-left)
    // - size: view size (w, h)
    // - stride: number of cells between starts of consecutive rows in the
    // backing storage
    constexpr BufferView(Cell *data, Size size, std::ptrdiff_t stride) noexcept
        : data_(data), size_(size), stride_(stride) {
    }

    [[nodiscard]] constexpr Size size() const noexcept {
      return size_;
    }
    [[nodiscard]] constexpr coord_t width() const noexcept {
      return size_.w;
    }
    [[nodiscard]] constexpr coord_t height() const noexcept {
      return size_.h;
    }

    // Bounds in view-local coordinates: origin always (0, 0).
    [[nodiscard]] constexpr Rect bounds() const noexcept {
      return Rect{Point{0, 0}, size_};
    }

    // Unsafe access (debug-checked). Coordinates are view-local.
    [[nodiscard]] Cell &at(coord_t x, coord_t y) noexcept {
      assert(in_bounds_(x, y));
      return data_[index_(x, y)];
    }
    [[nodiscard]] const Cell &at(coord_t x, coord_t y) const noexcept {
      assert(in_bounds_(x, y));
      return data_[index_(x, y)];
    }

    // Safe set: ignores out-of-bounds writes.
    constexpr void set(Point p, const Cell &c) noexcept {
      if (!bounds().contains(p))
        return;
      at(p.x, p.y) = c;
    }

    // Fill entire view with a cell value.
    void fill(const Cell &c) noexcept {
      if (size_.w <= 0 || size_.h <= 0)
        return;

      for (coord_t y = 0; y < size_.h; ++y) {
        Cell *row = data_ + y * stride_;
        std::fill(row, row + size_.w, c);
      }
    }

    // Fill a sub-rect (view-local) with clipping.
    void fill_rect(Rect r, const Cell &c) noexcept {
      r = r.intersect(bounds());
      if (r.empty())
        return;

      for (coord_t y = r.top(); y < r.bottom(); ++y) {
        Cell *row = data_ + y * stride_;
        std::fill(row + r.left(), row + r.right(), c);
      }
    }

    // Create a subview (clipped). Returned view may be empty.
    [[nodiscard]] BufferView subview(Rect r) const noexcept {
      r = r.intersect(bounds());
      if (r.empty())
        return {};

      Cell *ptr = data_ + index_(r.left(), r.top());
      return BufferView{ptr, Size{r.width(), r.height()}, stride_};
    }

  private:
    [[nodiscard]] constexpr bool
    in_bounds_(coord_t x, coord_t y) const noexcept {
      return (x >= 0 && y >= 0 && x < size_.w && y < size_.h);
    }

    [[nodiscard]] constexpr std::ptrdiff_t
    index_(coord_t x, coord_t y) const noexcept {
      return std::ptrdiff_t(y) * stride_ + std::ptrdiff_t(x);
    }

    Cell          *data_   = nullptr;
    Size           size_   = {0, 0};
    std::ptrdiff_t stride_ = 0; // in cells
  };

  // Todo.. Buffer: owning 2D cell buffer.
  class Buffer final {};
} // namespace glyph::core
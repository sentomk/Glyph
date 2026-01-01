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
#include <vector>

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
    // Return true if (x, y) lies within the view's local bounds [0, w) x [0, h)
    [[nodiscard]] constexpr bool
    in_bounds_(coord_t x, coord_t y) const noexcept {
      return (x >= 0 && y >= 0 && x < size_.w && y < size_.h);
    }

    // Convert a 2D local coordinate (x, y) into a linear index
    // using row-major storage with the given row stride.
    [[nodiscard]] constexpr std::ptrdiff_t
    index_(coord_t x, coord_t y) const noexcept {
      return std::ptrdiff_t(y) * stride_ + std::ptrdiff_t(x);
    }

    Cell          *data_   = nullptr;
    Size           size_   = {0, 0};
    std::ptrdiff_t stride_ = 0; // in cells
  };

  // Non-owning read-only 2D view into a buffer.
  // Semantics are identical to BufferView, except that mutation is forbidden.
  class ConstBufferView {
  public:
    using value_type = Cell;
    using pointer    = const Cell *;
    using reference  = const Cell &;

    // Constructs an empty view.
    constexpr ConstBufferView() noexcept = default;

    // Constructs a read-only view over raw cell storage.
    // stride is the number of cells between two consecutive rows.
    constexpr ConstBufferView(
        const Cell *data, Size size, std::ptrdiff_t stride) noexcept
        : data_(data), size_(size), stride_(stride) {
    }

    // Returns the logical size of the view.
    constexpr Size size() const noexcept {
      return size_;
    }

    // Returns the bounding rectangle of the view in local coordinates.
    constexpr Rect bounds() const noexcept {
      return Rect{0, 0, size_.w, size_.h};
    }

    // Returns true if the view contains no cells.
    constexpr bool empty() const noexcept {
      return data_ == nullptr || size_.empty();
    }

    // Accesses a cell at (x, y).
    // Behavior is undefined if (x, y) is out of bounds.
    constexpr const Cell &at(coord_t x, coord_t y) const noexcept {
      return data_[y * stride_ + x];
    }

    // Returns a clipped subview.
    // The resulting view is also read-only.
    constexpr ConstBufferView subview(Rect r) const noexcept {
      r = r.clipped(bounds());
      if (r.empty())
        return {};

      return ConstBufferView{
          data_ + r.top() * stride_ + r.left(),
          Size{r.width(), r.height()},
          stride_};
    }

  private:
    const Cell    *data_ = nullptr;
    Size           size_{};
    std::ptrdiff_t stride_ = 0;
  };

  // Buffer: owning 2D cell buffer.
  class Buffer final {
  public:
    using value_type = Cell;

    Buffer() = default;

    explicit Buffer(Size s, const Cell &fill_cell = Cell{})
        : size_(s.non_negative()),
          cells_(std::size_t(size_.w) * std::size_t(size_.h), fill_cell) {
    }

    [[nodiscard]] Size size() const noexcept {
      return size_;
    }
    [[nodiscard]] coord_t width() const noexcept {
      return size_.w;
    }
    [[nodiscard]] coord_t height() const noexcept {
      return size_.h;
    }

    // Bounds in buffer-local coordinates: origin always (0, 0).
    [[nodiscard]] Rect bounds() const noexcept {
      return Rect{Point{0, 0}, size_};
    }

    [[nodiscard]] bool empty() const noexcept {
      return size_.empty();
    }

    // Resize resets contents (simple and predictable). If you need preserving
    // resize,
    // implement it explicitly later; do not smuggle policy into core by
    // default.
    void resize(Size s, const Cell &fill_cell = Cell{}) {
      size_ = s.non_negative();
      cells_.assign(std::size_t(size_.w) * std::size_t(size_.h), fill_cell);
    }

    // Unsafe access (debug-checked).
    [[nodiscard]] Cell &at(coord_t x, coord_t y) noexcept {
      assert(bounds().contains(Point{x, y}));
      return cells_[index_(x, y)];
    }
    [[nodiscard]] const Cell &at(coord_t x, coord_t y) const noexcept {
      assert(bounds().contains(Point{x, y}));
      return cells_[index_(x, y)];
    }

    // Safe set: ignores out-of-bounds.
    void set(Point p, const Cell &c) noexcept {
      if (!bounds().contains(p))
        return;
      at(p.x, p.y) = c;
    }

    // Fill whole buffer.
    void fill(const Cell &c) noexcept {
      std::fill(cells_.begin(), cells_.end(), c);
    }

    // Fill a rect with clipping.
    void fill_rect(Rect r, const Cell &c) noexcept {
      view().fill_rect(r, c);
    }

    // Owning buffer -> view
    [[nodiscard]] BufferView view() noexcept {
      return BufferView{cells_.data(), size_, stride_()};
    }
    [[nodiscard]] BufferView view() const noexcept {
      // const-correct view: we return a mutable view type only if you want
      // mutation. For stricter const correctness, split into ConstBufferView
      // later.
      return BufferView{const_cast<Cell *>(cells_.data()), size_, stride_()};
    }

    // Subview (clipped). Returned view may be empty.
    [[nodiscard]] BufferView subview(Rect r) noexcept {
      return view().subview(r);
    }

  private:
    // Return the number of cells between starts of two consecutive rows.
    [[nodiscard]] std::ptrdiff_t stride_() const noexcept {
      return std::ptrdiff_t(size_.w);
    }

    // Convert a 2D buffer-local coordinate (x, y) into a linear index
    // for row-major contiguous storage.
    [[nodiscard]] std::size_t index_(coord_t x, coord_t y) const noexcept {
      return std::size_t(y) * std::size_t(size_.w) + std::size_t(x);
    }

    Size              size_{0, 0};
    std::vector<Cell> cells_{};
  };

} // namespace glyph::core
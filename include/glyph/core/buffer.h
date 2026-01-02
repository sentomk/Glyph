// glyph/core/buffer.h
//
// 2D cell buffer (the "canvas")
//
// Responsibilities:
//   - Own or view a rectangular 2D grid of Cells
//   - Provide safe read/write primitives
//   - Provide clipping against bounds
//
// Design notes:
//   - Buffer owns storage
//   - BufferView / ConstBufferView are non-owning, lightweight views
//   - const-correctness is a hard contract: const Buffer never yields writable
//   views

#pragma once

#include "cell.h"
#include "geometry.h"
#include "types.h"

#include <cassert>
#include <cstddef>
#include <vector>

namespace glyph::core {

  // ------------------------------------------------------------
  // ConstBufferView: read-only, non-owning 2D view
  // ------------------------------------------------------------
  struct ConstBufferView final {
    const Cell    *data = nullptr;
    Size           size{};
    std::ptrdiff_t stride = 0; // elements per row

    constexpr ConstBufferView() noexcept = default;

    constexpr ConstBufferView(const Cell *d, Size s, std::ptrdiff_t st) noexcept
        : data(d), size(s), stride(st) {
    }

    [[nodiscard]] constexpr bool empty() const noexcept {
      return !data || size.w <= 0 || size.h <= 0;
    }

    [[nodiscard]] constexpr Rect bounds() const noexcept {
      return Rect{Point{0, 0}, size};
    }

    [[nodiscard]] constexpr const Cell &
    at(coord_t x, coord_t y) const noexcept {
      assert(x >= 0 && x < size.w);
      assert(y >= 0 && y < size.h);
      return data[std::size_t(y) * std::size_t(stride) + std::size_t(x)];
    }

    [[nodiscard]] constexpr ConstBufferView subview(Rect r) const noexcept {
      if (empty())
        return {};

      const Rect clipped = r.intersect(bounds());
      if (clipped.empty())
        return {};

      const std::size_t offset =
          std::size_t(clipped.origin.y) * std::size_t(stride) +
          std::size_t(clipped.origin.x);

      return ConstBufferView{
          data + offset,
          clipped.size,
          stride,
      };
    }
  };

  // ------------------------------------------------------------
  // BufferView: writable, non-owning 2D view
  // ------------------------------------------------------------
  struct BufferView final {
    Cell          *data = nullptr;
    Size           size{};
    std::ptrdiff_t stride = 0;

    constexpr BufferView() noexcept = default;

    constexpr BufferView(Cell *d, Size s, std::ptrdiff_t st) noexcept
        : data(d), size(s), stride(st) {
    }

    [[nodiscard]] constexpr bool empty() const noexcept {
      return !data || size.w <= 0 || size.h <= 0;
    }

    [[nodiscard]] constexpr Rect bounds() const noexcept {
      return Rect{Point{0, 0}, size};
    }

    [[nodiscard]] constexpr Cell &at(coord_t x, coord_t y) noexcept {
      assert(x >= 0 && x < size.w);
      assert(y >= 0 && y < size.h);
      return data[std::size_t(y) * std::size_t(stride) + std::size_t(x)];
    }

    [[nodiscard]] constexpr const Cell &
    at(coord_t x, coord_t y) const noexcept {
      assert(x >= 0 && x < size.w);
      assert(y >= 0 && y < size.h);
      return data[std::size_t(y) * std::size_t(stride) + std::size_t(x)];
    }

    [[nodiscard]] constexpr BufferView subview(Rect r) const noexcept {
      if (empty())
        return {};

      const Rect clipped = r.intersect(bounds());
      if (clipped.empty())
        return {};

      const std::size_t offset =
          std::size_t(clipped.origin.y) * std::size_t(stride) +
          std::size_t(clipped.origin.x);

      return BufferView{
          data + offset,
          clipped.size,
          stride,
      };
    }

    [[nodiscard]] constexpr ConstBufferView const_view() const noexcept {
      return ConstBufferView{data, size, stride};
    }
  };

  // ------------------------------------------------------------
  // Buffer: owning 2D cell storage
  // ------------------------------------------------------------
  class Buffer final {
  public:
    Buffer() = default;

    explicit Buffer(Size s)
        : size_(s), cells_(std::size_t(s.w) * std::size_t(s.h)) {
    }

    [[nodiscard]] Size size() const noexcept {
      return size_;
    }

    [[nodiscard]] bool empty() const noexcept {
      return size_.w <= 0 || size_.h <= 0;
    }

    [[nodiscard]] BufferView view() noexcept {
      return BufferView{
          cells_.data(),
          size_,
          stride_(),
      };
    }

    [[nodiscard]] ConstBufferView const_view() const noexcept {
      return ConstBufferView{
          cells_.data(),
          size_,
          stride_(),
      };
    }

    // Backward-compatible name if you want to keep call sites stable.
    // Prefer const_view() at new call sites.
    [[nodiscard]] ConstBufferView view() const noexcept {
      return const_view();
    }

  private:
    [[nodiscard]] std::ptrdiff_t stride_() const noexcept {
      return std::ptrdiff_t(size_.w);
    }

    Size              size_{0, 0};
    std::vector<Cell> cells_{};
  };

} // namespace glyph::core

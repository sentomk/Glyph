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

#include <algorithm>
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

    // Fill entire view with a cell.
    void clear(const Cell &c = Cell{}) noexcept {
      if (empty())
        return;

      for (coord_t y = 0; y < size.h; ++y) {
        for (coord_t x = 0; x < size.w; ++x) {
          at(x, y) = c;
        }
      }
    }

    // Fill a rect (clipped) with a cell.
    void fill_rect(Rect r, const Cell &c) noexcept {
      auto sub = subview(r);
      if (sub.empty())
        return;

      for (coord_t y = 0; y < sub.size.h; ++y) {
        for (coord_t x = 0; x < sub.size.w; ++x) {
          sub.at(x, y) = c;
        }
      }
    }

    // Blit from a const view into this view at dst.
    void blit(ConstBufferView src, Point dst) noexcept {
      if (empty() || src.empty())
        return;

      Rect dst_rect{dst, src.size};
      Rect clipped = dst_rect.intersect(bounds());
      if (clipped.empty())
        return;

      // Compute source start offset after clipping.
      const coord_t sx0 = coord_t(clipped.origin.x - dst.x);
      const coord_t sy0 = coord_t(clipped.origin.y - dst.y);

      for (coord_t y = 0; y < clipped.size.h; ++y) {
        for (coord_t x = 0; x < clipped.size.w; ++x) {
          const auto &c = src.at(sx0 + x, sy0 + y);
          at(clipped.origin.x + x, clipped.origin.y + y) = c;
        }
      }
    }

    // Write a cell with width-aware placement.
    void put(Point p, Cell c) noexcept {
      if (p.x < 0 || p.y < 0 || p.x >= size.w || p.y >= size.h)
        return;

      // If overwriting a wide glyph's lead cell, clear its spacer.
      {
        const auto &cur = at(p.x, p.y);
        if (cur.width == 2 && p.x + 1 < size.w) {
          at(p.x + 1, p.y) = Cell{};
        }
        // If overwriting a spacer cell, clear the left wide glyph.
        if (cur.width == 0 && p.x > 0) {
          auto &left = at(p.x - 1, p.y);
          if (left.width == 2) {
            left = Cell{};
          }
        }
      }

      if (c.width == 2) {
        // If no space for wide glyph, degrade to width=1.
        if (p.x + 1 >= size.w) {
          c.width      = 1;
          at(p.x, p.y) = c;
          return;
        }

        at(p.x, p.y) = c;

        Cell spacer{};
        spacer.ch        = U'\0';
        spacer.width     = 0;
        spacer.style     = c.style;
        at(p.x + 1, p.y) = spacer;
        return;
      }

      at(p.x, p.y) = c;
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

    // Clear whole buffer.
    void clear(const Cell &c = Cell{}) noexcept {
      view().clear(c);
    }

    // Fill a rect (clipped).
    void fill_rect(Rect r, const Cell &c) noexcept {
      view().fill_rect(r, c);
    }

    // Blit from a const view into this buffer at dst.
    void blit(ConstBufferView src, Point dst) noexcept {
      view().blit(src, dst);
    }

    // Resize buffer, preserving overlapping region.
    void resize(Size s, const Cell &fill = Cell{}) {
      if (s.w <= 0 || s.h <= 0) {
        size_ = s;
        cells_.clear();
        return;
      }

      std::vector<Cell> next(std::size_t(s.w) * std::size_t(s.h), fill);

      const coord_t copy_w = std::min(size_.w, s.w);
      const coord_t copy_h = std::min(size_.h, s.h);

      for (coord_t y = 0; y < copy_h; ++y) {
        for (coord_t x = 0; x < copy_w; ++x) {
          const std::size_t dst_i =
              std::size_t(y) * std::size_t(s.w) + std::size_t(x);
          const std::size_t src_i =
              std::size_t(y) * std::size_t(size_.w) + std::size_t(x);
          next[dst_i] = cells_[src_i];
        }
      }

      size_ = s;
      cells_.swap(next);
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

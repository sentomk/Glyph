// glyph/core/diff.h
//
// Buffer diff helpers.
//
// Responsibilities:
//  - Compute line hashes for quick change detection.
//  - Generate minimal changes spans between two buffers.
//  - Provide a simple path for dirty-line acceleration.

#pragma once

#include "buffer.h"
#include "geometry.h"
#include "types.h"

#include <cstdint>
#include <span>
#include <vector>

namespace glyph::core {

  using line_hash_t = std::uint64_t;

  // A horizontal span on a specific row.
  // Range is [x0, x1), half-open.
  struct DiffSpan final {
    coord_t y  = 0;
    coord_t x0 = 0;
    coord_t x1 = 0;

    [[nodiscard]] constexpr bool empty() const noexcept {
      return x1 <= x0;
    }
  };

  // ------------------------------------------------------------
  // FNV-1a helpers
  // ------------------------------------------------------------
  constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
  constexpr std::uint64_t kFnvPrime  = 1099511628211ull;

  inline void fnv_add(std::uint64_t &h, std::uint64_t v) noexcept {
    for (int i = 0; i < 8; ++i) {
      const std::uint8_t b = std::uint8_t((v >> (i * 8)) & 0xffu);
      h ^= b;
      h *= kFnvPrime;
    }
  }

  inline std::uint64_t hash_cell(const Cell &c) noexcept {
    std::uint64_t h = kFnvOffset;

    fnv_add(h, std::uint64_t(c.ch));
    fnv_add(h, std::uint64_t(c.width));
    fnv_add(h, std::uint64_t(c.style.fg));
    fnv_add(h, std::uint64_t(c.style.bg));
    fnv_add(h, std::uint64_t(c.style.attrs));

    return h;
  }

  inline line_hash_t hash_line(ConstBufferView v, coord_t y) noexcept {
    std::uint64_t h = kFnvOffset;
    for (coord_t x = 0; x < v.size.w; ++x) {
      fnv_add(h, hash_cell(v.at(x, y)));
    }
    return h;
  }

  // Compute per-line hashes for a buffer view.
  inline std::vector<line_hash_t> line_hashes(ConstBufferView v) {
    std::vector<line_hash_t> out;
    if (v.empty())
      return out;

    out.resize(std::size_t(v.size.h));
    for (coord_t y = 0; y < v.size.h; ++y) {
      out[std::size_t(y)] = hash_line(v, y);
    }
    return out;
  }

  // Return lines that differ between prev and next.
  // If sizes differ, all lines in next are treated as dirty.
  inline std::vector<coord_t>
  diff_lines(ConstBufferView prev, ConstBufferView next) {
    std::vector<coord_t> dirty;

    if (prev.size.w != next.size.w || prev.size.h != next.size.h) {
      for (coord_t y = 0; y < next.size.h; ++y) {
        dirty.push_back(y);
      }
      return dirty;
    }

    for (coord_t y = 0; y < next.size.h; ++y) {
      if (hash_line(prev, y) != hash_line(next, y)) {
        dirty.push_back(y);
      }
    }
    return dirty;
  }

  // Compute minimal changed spans on specific lines.
  inline std::vector<DiffSpan> diff_spans(
      ConstBufferView          prev,
      ConstBufferView          next,
      std::span<const coord_t> lines) {
    std::vector<DiffSpan> spans;

    if (prev.size.w != next.size.w || prev.size.h != next.size.h) {
      for (coord_t y = 0; y < next.size.h; ++y) {
        spans.push_back(DiffSpan{y, 0, next.size.w});
      }
      return spans;
    }

    for (coord_t y : lines) {
      if (y < 0 || y >= next.size.h)
        continue;

      coord_t x = 0;
      while (x < next.size.w) {
        if (prev.at(x, y) == next.at(x, y)) {
          ++x;
          continue;
        }

        const coord_t x0 = x;
        while (x < next.size.w && prev.at(x, y) != next.at(x, y)) {
          ++x;
        }
        spans.push_back(DiffSpan{y, x0, x});
      }
    }

    return spans;
  }

  // Compute minimal changed spans for all differing lines.
  inline std::vector<DiffSpan>
  diff_spans(ConstBufferView prev, ConstBufferView next) {
    const auto lines = diff_lines(prev, next);
    return diff_spans(prev, next, std::span<const coord_t>(lines));
  }

} // namespace glyph::core
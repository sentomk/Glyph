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

namespace glyph::core {

  // ------------------------------------------------------------
  // BufferView: non-owning view over a (sub)rectangle of a buffer-like storage.
  // Row-major contiguous storage assumed, with a configurable row stride.
  // ------------------------------------------------------------
  class BufferView final {
  public:
    using value_type = Cell;

    constexpr BufferView() noexcept = default;
  };
} // namespace glyph::core
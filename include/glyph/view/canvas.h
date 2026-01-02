// glyph/view/canvas.h
//
// Canvas: a restricted, local drawing context.
//
// A Canvas represents a writable sub-region of a Frame with its own
// local coordinate system (0,0 at top-left of the region).
//
// Responsibilities:
//  - Provide safe, clipped drawing primitives for view-layer code
//  - Hide global frame coordinates and buffer layout details

#pragma once

#include "glyph/core/buffer.h"
#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/core/types.h"

namespace glyph::view {

  // ------------------------------------------------------------
  // Canvas: local, clipped drawing context.
  // ------------------------------------------------------------
  class Canvas final {
  public:
    using cell_type = core::Cell;

    Canvas() = default;

    explicit Canvas(core::BufferView v) noexcept : view_(v) {
    }

    [[nodiscard]] core::Size size() const noexcept {
      return view_.size;
    }

    [[nodiscard]] bool empty() const noexcept {
      return view_.empty();
    }

    // Safe set: ignores out-of bounds writes.
    void set(core::Point p, const cell_type &c) noexcept {
      if (p.x < 0 || p.y < 0)
        return;
      if (p.x >= view_.size.w || p.y >= view_.size.h)
        return;

      view_.at(p.x, p.y) = c;
    }

    // Fill entire canvas.
    void fill(const cell_type &c) noexcept {
      if (view_.empty())
        return;

      for (core::coord_t y = 0; y < view_.size.h; ++y) {
        for (core::coord_t x = 0; x < view_.size.w; ++x) {
          view_.at(x, y) = c;
        }
      }
    }

    // Fill a rectangle in local canvas coordinate.
    void fill_rect(core::Rect r, const cell_type &c) noexcept {
      auto sub = view_.subview(r);
      if (sub.empty())
        return;

      for (core::coord_t y = 0; y < sub.size.h; ++y) {
        for (core::coord_t x = 0; x < sub.size.w; ++x) {
          sub.at(x, y) = c;
        }
      }
    }

  private:
    core::BufferView view_;
  };
} // namespace glyph::view
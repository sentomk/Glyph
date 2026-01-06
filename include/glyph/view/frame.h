// glyph/view/frame.h
//
// Semantic frame (one render pass "canvas")
//
// Responsibilities:
//  - Own a core::Buffer
//  - Provide a controlled mutation entry for view-layer code
//  - Provide clipping/subview helpers

#pragma once

#include "canvas.h"
#include "glyph/core/buffer.h"
#include "glyph/core/geometry.h"

namespace glyph::view {

  // ------------------------------------------------------------
  // Frame: owning semantic drawing surface for a single pass.
  // ------------------------------------------------------------
  class Frame final {
  public:
    using cell_type        = core::Cell;
    using buffer_type      = core::Buffer;
    using buffer_view_type = core::BufferView;
    using const_view_type  = core::ConstBufferView;

    Frame() = default;

    explicit Frame(core::Size s, const cell_type &fill_cell = cell_type{})
        : buf_(s) {
      if (!buf_.empty()) {
        fill(fill_cell);
      }
    }

    [[nodiscard]] core::Size size() const noexcept {
      return buf_.size();
    }

    [[nodiscard]] bool empty() const noexcept {
      return buf_.empty();
    }

    // Bounds in frame-local coordinates: origin always (0, 0).
    [[nodiscard]] core::Rect bounds() const noexcept {
      return core::Rect{core::Point{0, 0}, buf_.size()};
    }

    // Unsafe access (debug-checked).
    [[nodiscard]] cell_type &at(core::coord_t x, core::coord_t y) noexcept {
      return buf_.view().at(x, y);
    }
    [[nodiscard]] const cell_type &
    at(core::coord_t x, core::coord_t y) const noexcept {
      return buf_.const_view().at(x, y);
    }

    // Safe set: ignores out-of-bounds writes.
    void set(core::Point p, const cell_type &c) noexcept {
      if (!bounds().contains(p))
        return;

      buf_.view().put(p, c);
    }

    // Frame -> view (mutable)
    [[nodiscard]] buffer_view_type view() noexcept {
      return buf_.view();
    }

    // Frame -> view (read-only)
    [[nodiscard]] const_view_type view() const noexcept {
      return buf_.const_view();
    }

    // Fill whole frame.
    void fill(const cell_type &c) noexcept {
      buf_.view().clear(c);
    }

    // Fill a rect with clipping.
    void fill_rect(core::Rect r, const cell_type &c) noexcept {
      buf_.view().fill_rect(r, c);
    }

    // Subview (clipped). Returned view may be empty.
    [[nodiscard]] buffer_view_type subview(core::Rect r) noexcept {
      return buf_.view().subview(r);
    }

    // Generate a Canvas.
    [[nodiscard]] Canvas canvas(core::Rect area) noexcept {
      return Canvas{buf_.view().subview(area)};
    }

    // Sub-frame view with local coordinates (alias for Canvas).
    [[nodiscard]] Canvas sub_frame(core::Rect area) noexcept {
      return Canvas{buf_.view().subview(area)};
    }

    std::vector<core::coord_t> take_dirty_lines() const {
      return buf_.take_dirty_lines();
    }

  private:
    buffer_type buf_{};
  };

} // namespace glyph::view

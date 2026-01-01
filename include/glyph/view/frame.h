// glyph/view/frame.h
// Semantic frame (one render pass "canvas")
//
// Responsibilities:
//  - Own a core::Buffer
//  - Provide a controlled mutation entry for view-layer code
//  - Provide clipping/subview helpers

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
        : buf_(s, fill_cell) {
    }

    [[nodiscard]] core::Size size() const noexcept {
      return buf_.size();
    }
    [[nodiscard]] core::coord_t width() const noexcept {
      return buf_.width();
    }
    [[nodiscard]] core::coord_t height() const noexcept {
      return buf_.height();
    }

    // Bounds in frame-local coordinates: origin always (0, 0).
    [[nodiscard]] core::Rect bounds() const noexcept {
      return buf_.bounds();
    }

    [[nodiscard]] bool empty() const noexcept {
      return buf_.empty();
    }

    // Unsafe access (debug-checked).
    [[nodiscard]] cell_type &at(core::coord_t x, core::coord_t y) noexcept {
      return buf_.at(x, y);
    }
    [[nodiscard]] const cell_type &
    at(core::coord_t x, core::coord_t y) const noexcept {
      return buf_.at(x, y);
    }

    // Safe set: ignores out-of-bounds writes.
    void set(core::Point p, const cell_type &c) noexcept {
      buf_.set(p, c);
    }

    // Fill whole frame.
    void fill(const cell_type &c) noexcept {
      buf_.fill(c);
    }

    // Fill a rect with clipping.
    void fill_rect(core::Rect r, const cell_type &c) noexcept {
      buf_.fill_rect(r, c);
    }

    // Frame -> view (mutable)
    [[nodiscard]] buffer_view_type view() noexcept {
      return buf_.view();
    }

    // Frame -> view (read-only)
    [[nodiscard]] const_view_type view() const noexcept {
      // Prefer strict const view here.
      // core::Buffer::view() currently returns BufferView even for const,
      // so we build a ConstBufferView explicitly.
      auto v = buf_.view();
      return const_view_type{&v.at(0, 0), v.size(), std::ptrdiff_t(v.width())};
    }

    // Subview (clipped). Returned view may be empty.
    [[nodiscard]] buffer_view_type subview(core::Rect r) noexcept {
      return buf_.subview(r);
    }

  private:
    buffer_type buf_{};
  };

} // namespace glyph::view

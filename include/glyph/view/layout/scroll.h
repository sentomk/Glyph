// glyph/view/layout/scroll.h
//
// Scroll model helpers.
//
// Responsibilities:
//   - Track content size, viewport size, and scroll offset.
//   - Clamp and adjust offset for common operations.

#pragma once

#include <algorithm>

#include "glyph/core/types.h"

namespace glyph::view::layout {

  // ------------------------------------------------------------
  // ScrollModel (1D)
  // ------------------------------------------------------------
  // Use one model per axis when building 2D scroll views.
  struct ScrollModel final {
    core::coord_t content  = 0; // total content length
    core::coord_t viewport = 0; // visible length
    core::coord_t offset   = 0; // first visible index

    void set_content(core::coord_t value) {
      content = std::max<core::coord_t>(0, value);
      clamp_offset();
    }

    void set_viewport(core::coord_t value) {
      viewport = std::max<core::coord_t>(0, value);
      clamp_offset();
    }

    void set_offset(core::coord_t value) {
      offset = value;
      clamp_offset();
    }

    void scroll_by(core::coord_t delta) {
      offset = core::coord_t(offset + delta);
      clamp_offset();
    }

    void scroll_to_start() {
      offset = 0;
    }

    void scroll_to_end() {
      offset = max_offset();
    }

    core::coord_t max_offset() const {
      return std::max<core::coord_t>(0, content - viewport);
    }

    core::coord_t visible_start() const {
      return offset;
    }

    core::coord_t visible_end() const {
      return core::coord_t(offset + viewport);
    }

    bool ensure_visible(core::coord_t pos, core::coord_t span = 1) {
      if (span < 1) {
        span = 1;
      }

      if (pos < 0) {
        pos = 0;
      }

      core::coord_t next = offset;
      if (pos < offset) {
        next = pos;
      }
      else if (pos + span > offset + viewport) {
        next = core::coord_t(pos + span - viewport);
      }

      if (next < 0) {
        next = 0;
      }

      const auto max_off = max_offset();
      if (next > max_off) {
        next = max_off;
      }

      if (next == offset) {
        return false;
      }

      offset = next;
      return true;
    }

  private:
    void clamp_offset() {
      const auto max_off = max_offset();
      if (offset < 0) {
        offset = 0;
      }
      else if (offset > max_off) {
        offset = max_off;
      }
    }
  };

} // namespace glyph::view::layout

// glyph/view/components/focus.h
//
// Focus/selection models for view components.
//
// Responsibilities:
//   - Track focused index for a set of focusable items.
//   - Track a single selected index for list/table views.

#pragma once

#include <algorithm>

#include "glyph/core/types.h"

namespace glyph::view {

  // ------------------------------------------------------------
  // FocusModel (single focus index)
  // ------------------------------------------------------------
  struct FocusModel final {
    core::coord_t count   = 0;
    core::coord_t focused = 0;

    void set_count(core::coord_t value) {
      count = std::max<core::coord_t>(0, value);
      clamp();
    }

    void set_focused(core::coord_t index) {
      focused = index;
      clamp();
    }

    void next() {
      if (count <= 0) {
        focused = 0;
        return;
      }
      focused = core::coord_t((focused + 1) % count);
    }

    void prev() {
      if (count <= 0) {
        focused = 0;
        return;
      }
      focused = core::coord_t((focused - 1 + count) % count);
    }

    bool is_focused(core::coord_t index) const {
      return count > 0 && focused == index;
    }

  private:
    void clamp() {
      if (count <= 0) {
        focused = 0;
        return;
      }
      if (focused < 0) {
        focused = 0;
      }
      else if (focused >= count) {
        focused = core::coord_t(count - 1);
      }
    }
  };

  // ------------------------------------------------------------
  // SelectionModel (single selection)
  // ------------------------------------------------------------
  struct SelectionModel final {
    core::coord_t count    = 0;
    core::coord_t selected = -1; // -1 = none

    void set_count(core::coord_t value) {
      count = std::max<core::coord_t>(0, value);
      clamp();
    }

    void set_selected(core::coord_t index) {
      selected = index;
      clamp();
    }

    void clear() {
      selected = -1;
    }

    bool has_selection() const {
      return selected >= 0 && selected < count;
    }

    bool is_selected(core::coord_t index) const {
      return has_selection() && selected == index;
    }

  private:
    void clamp() {
      if (count <= 0) {
        selected = -1;
        return;
      }
      if (selected < 0) {
        selected = -1;
      }
      else if (selected >= count) {
        selected = core::coord_t(count - 1);
      }
    }
  };

} // namespace glyph::view

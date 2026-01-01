// glyph/view/view.h
//
// View abstraction (semantic draw intent)
//
// Responsibilities:
//   - Define the minimal render interface for view-layer code
//   - Keep view independent from backend/IO and layout policy
//   - Enable composition by calling render(frame, area) repeatedly

#pragma once

#include "glyph/core/geometry.h"

namespace glyph::view {

  class Frame;

  // ------------------------------------------------------------
  // View: minimal semantic draw unit.
  //
  // Contract:
  //   - Must render within the given 'area' only.
  //   - Must not perform IO or depend on backend.
  //   - Must tolerate empty/degenerate areas.
  // ------------------------------------------------------------
  struct View {
    virtual ~View()                                      = default;
    virtual void render(Frame &f, core::Rect area) const = 0;
  };

} // namespace glyph::view
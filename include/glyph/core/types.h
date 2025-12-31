// glyph/core/types.h
//
// Core fundamental type aliases used across Glyph.
// This file exists to centralize low-level decisions so they do not
// silently diverge across modules.

#pragma once

#include <cstdint>

namespace glyph::core {
  // Coordinate type.
  //
  // Int32 is plenty for typical terminal size while keeping operations cheap.
  using coord_t = std::int32_t;

} // namespace glyph::core
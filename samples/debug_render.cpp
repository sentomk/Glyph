// examples/debug_render.cpp
//
// Minimal program to validate the core -> view -> render pipeline
// using DebugRenderer.

#include <iostream>

#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/core/types.h"
#include "glyph/render/render.h"
#include "glyph/view/frame.h"
#include "glyph/render/debug/debug_renderer.h"
#include "glyph/view/view.h"

namespace {

  // ------------------------------------------------------------
  // HelloView: a minimal View impl.
  // Paints a border-like fill and writes "Glyph" inside "area".
  // ------------------------------------------------------------
  class HelloView final : public glyph::view::View {
  public:
    void render(glyph::view::Frame &f, glyph::core::Rect area) const override {
      using namespace glyph;

      if (area.empty())
        return;

      // Fill background in the given area only.
      core::Cell dot = core::Cell::from_char(u'.');
      f.fill_rect(area, dot);

      // Write a label, clipped by area bounds.
      const char32_t text[] = U"Glyph";

      const core::coord_t x0 = area.left() + 2;
      const core::coord_t y0 = area.top() + 2;

      for (core::coord_t i = 0; text[i] != U'\0'; ++i) {
        const core::Point p{x0 + i, y0};
        core::Cell        c = core::Cell::from_char(text[i]);

        // safe set
        f.set(p, c);
      }
    }
  };
} // namespace

int main() {
  using namespace glyph;

  // ------------------------------------------------------------
  // Stage 1: create frame
  // ------------------------------------------------------------
  view::Frame frame{core::Size{16, 4}};

  // ------------------------------------------------------------
  // Stage 2: view paints into frame within a specific area
  // ------------------------------------------------------------
  HelloView v;
  v.render(frame, frame.bounds());

  // ------------------------------------------------------------
  // Stage 3: render consumes frame (read-only)
  // ------------------------------------------------------------
  render::DebugRenderer r{std::cout};
  r.render(frame);

  return 0;
}
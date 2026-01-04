// samples/layout_demo.cpp
//
// Layout demo: renders layout rectangles as different glyph fills.
// Renderer split: choose DebugRenderer or AnsiRenderer.

#include <iostream>

#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/render/ansi/ansi_renderer.h"
#include "glyph/render/debug/debug_renderer.h"
#include "glyph/view/frame.h"

#include "glyph/view/layout/box.h"
#include "glyph/view/layout/inset.h"
#include "glyph/view/layout/split.h"
#include "glyph/view/layout/align.h"

namespace {

  using namespace glyph;

  void fill_rect(view::Frame &f, core::Rect r, char32_t ch) {
    f.fill_rect(r, core::Cell::from_char(ch));
  }

  view::Frame build_frame() {
    const core::Size size{36, 12};
    view::Frame      frame{size};
    frame.fill(core::Cell::from_char(U'.'));

    // 1) Inset the whole area to create a padding ring.
    auto inner =
        view::layout::inset_rect(frame.bounds(), view::layout::Insets::all(1));

    // 2) Box layout: 3 columns.
    view::layout::BoxItem cols[] = {
        {8, 0},  // fixed
        {-1, 1}, // flex 1
        {-1, 2}, // flex 2
    };

    auto col_rects = view::layout::layout_box(
        view::layout::Axis::Horizontal, inner, cols, 1);

    // Column fills
    if (col_rects.rects.size() >= 3) {
      fill_rect(frame, col_rects.rects[0], U'A');

      // 3) Split the middle column vertically into two.
      view::layout::SplitRatio ratios[] = {{1}, {1}};
      auto                     mid      = view::layout::layout_split_ratio(
          view::layout::Axis::Vertical, col_rects.rects[1], ratios, 1);

      if (mid.rects.size() >= 2) {
        fill_rect(frame, mid.rects[0], U'B');
        fill_rect(frame, mid.rects[1], U'C');
      }

      // 4) Align a smaller rect inside the right column.
      view::layout::AlignSpec spec{};
      spec.h    = view::layout::AlignH::Center;
      spec.v    = view::layout::AlignV::Center;
      spec.size = core::Size{6, 3};

      auto aligned = view::layout::align_rect(col_rects.rects[2], spec);
      fill_rect(frame, col_rects.rects[2], U'D');
      fill_rect(frame, aligned, U'X');
    }

    return frame;
  }

  void render_debug(const view::Frame &frame) {
    render::DebugRenderer r{std::cout};
    r.render(frame);
  }

  void render_ansi(const view::Frame &frame) {
    render::AnsiRenderer r{std::cout};
    r.render(frame);
  }

} // namespace

int main() {
  const auto frame = build_frame();

  const bool use_ansi = true;
  if (use_ansi) {
    render_ansi(frame);
  }
  else {
    render_debug(frame);
  }

  return 0;
}

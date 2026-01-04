// samples/basic/layout_gallery.cpp
//
// Layout gallery demo: shows box, split, align, inset, and stack layouts.

#include <iostream>

#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/render/ansi/ansi_renderer.h"
#include "glyph/render/debug/debug_renderer.h"
#include "glyph/view/frame.h"

#include "glyph/view/layout/align.h"
#include "glyph/view/layout/box.h"
#include "glyph/view/layout/inset.h"
#include "glyph/view/layout/split.h"
#include "glyph/view/layout/stack.h"

namespace {

  using namespace glyph;

  void fill_rect(view::Frame &f, core::Rect r, char32_t ch) {
    if (r.empty())
      return;
    f.fill_rect(r, core::Cell::from_char(ch));
  }

  void draw_border(view::Frame &f, core::Rect r, char32_t ch) {
    if (r.empty())
      return;

    const core::coord_t x0 = r.left();
    const core::coord_t y0 = r.top();
    const core::coord_t x1 = r.right() - 1;
    const core::coord_t y1 = r.bottom() - 1;

    if (x0 > x1 || y0 > y1)
      return;

    for (core::coord_t x = x0; x <= x1; ++x) {
      f.set(core::Point{x, y0}, core::Cell::from_char(ch));
      f.set(core::Point{x, y1}, core::Cell::from_char(ch));
    }

    for (core::coord_t y = y0; y <= y1; ++y) {
      f.set(core::Point{x0, y}, core::Cell::from_char(ch));
      f.set(core::Point{x1, y}, core::Cell::from_char(ch));
    }
  }

  view::Frame build_frame() {
    const core::Size size{56, 16};
    view::Frame      frame{size};
    frame.fill(core::Cell::from_char(U'.'));

    const auto outer =
        view::layout::inset_rect(frame.bounds(), view::layout::Insets::all(1));

    // Split into top and bottom bands.
    const auto bands = view::layout::layout_split_fixed(
        view::layout::Axis::Vertical, outer, 6, 1);

    if (bands.rects.size() >= 2) {
      // Top band: box layout with fixed + flex items.
      view::layout::BoxItem items[] = {
          {8, 0},  // fixed
          {-1, 1}, // flex 1
          {-1, 2}, // flex 2
          {6, 0},  // fixed
      };

      auto boxes = view::layout::layout_box(
          view::layout::Axis::Horizontal, bands.rects[0], items, 1);

      if (boxes.rects.size() >= 4) {
        fill_rect(frame, boxes.rects[0], U'A');
        fill_rect(frame, boxes.rects[1], U'B');
        fill_rect(frame, boxes.rects[2], U'C');
        fill_rect(frame, boxes.rects[3], U'D');
      }

      // Bottom band: split into left/right areas.
      view::layout::SplitRatio ratios[] = {{2}, {3}};
      auto                     columns = view::layout::layout_split_ratio(
          view::layout::Axis::Horizontal, bands.rects[1], ratios, 2);

      if (columns.rects.size() >= 2) {
        // Left: align demo.
        fill_rect(frame, columns.rects[0], U'L');
        view::layout::AlignSpec spec{};
        spec.h    = view::layout::AlignH::Center;
        spec.v    = view::layout::AlignV::Center;
        spec.size = core::Size{10, 3};
        fill_rect(frame, view::layout::align_rect(columns.rects[0], spec), U'X');

        // Right: stack demo with inset borders.
        fill_rect(frame, columns.rects[1], U'R');
        auto layers = view::layout::layout_stack(columns.rects[1], 3);
        for (std::size_t i = 0; i < layers.rects.size(); ++i) {
          const auto inset = view::layout::inset_rect(
              layers.rects[i], view::layout::Insets::all(
                                   static_cast<core::coord_t>(i)));
          const char32_t ch = static_cast<char32_t>(U'1' + i);
          draw_border(frame, inset, ch);
        }
      }
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
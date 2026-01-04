// samples/basic/components_demo.cpp
//
// Component demo: shows FillView, BorderView, and LabelView.

#include <iostream>

#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/render/ansi/ansi_renderer.h"
#include "glyph/view/frame.h"
#include "glyph/view/view.h"

#include "glyph/view/components/border.h"
#include "glyph/view/components/fill.h"
#include "glyph/view/components/label.h"
#include "glyph/view/layout/box.h"
#include "glyph/view/layout/inset.h"
#include "glyph/view/layout/split.h"

namespace {

  using namespace glyph;

  class BoxView final : public view::View {
  public:
    BoxView(view::FillView fill, view::BorderView border)
        : fill_(fill), border_(border) {
    }

    void render(view::Frame &f, core::Rect area) const override {
      if (area.empty())
        return;

      fill_.render(f, area);
      border_.render(f, area);

      // Inner panel for contrast.
      const auto inner =
          view::layout::inset_rect(area, view::layout::Insets::all(1));
      fill_.render(f, inner);
    }

  private:
    view::FillView   fill_;
    view::BorderView border_;
  };

} // namespace

int main() {
  using namespace glyph;

  view::Frame frame{core::Size{60, 14}};
  frame.fill(core::Cell::from_char(U'.'));

  const auto area =
      view::layout::inset_rect(frame.bounds(), view::layout::Insets::all(1));

  view::layout::BoxItem cols[] = {
      {-1, 1},
      {-1, 1},
      {-1, 1},
  };

  auto col_rects = view::layout::layout_box(
      view::layout::Axis::Horizontal, area, cols, 1);

  if (col_rects.rects.size() >= 3) {
    // Left: pure fill.
    view::FillView fill_left(core::Cell::from_char(U'L'));
    fill_left.render(frame, col_rects.rects[0]);

    // Middle: border + fill composition.
    view::FillView   fill_mid(core::Cell::from_char(U' '));
    view::BorderView border(core::Cell::from_char(U'#'));
    BoxView          box(fill_mid, border);
    box.render(frame, col_rects.rects[1]);

    // Right: label behaviors (wrap vs ellipsis).
    view::FillView   fill_right(core::Cell::from_char(U' '));
    view::BorderView right_border(core::Cell::from_char(U'='));
    fill_right.render(frame, col_rects.rects[2]);
    right_border.render(frame, col_rects.rects[2]);

    view::layout::SplitRatio halves[] = {{1}, {1}};
    auto label_rows = view::layout::layout_split_ratio(
        view::layout::Axis::Vertical, col_rects.rects[2], halves, 1);

    if (label_rows.rects.size() >= 2) {
      const auto top = view::layout::inset_rect(
          label_rows.rects[0], view::layout::Insets::all(1));
      const auto bottom = view::layout::inset_rect(
          label_rows.rects[1], view::layout::Insets::all(1));

      view::LabelView wrap_label(
          U"Wrap: The quick brown fox jumps over the lazy dog.");
      wrap_label.set_align(view::layout::AlignH::Left,
                           view::layout::AlignV::Top);
      wrap_label.set_wrap(true);
      wrap_label.render(frame, top);

      view::LabelView ellipsis_label(
          U"Ellipsis: The quick brown fox jumps over the lazy dog.");
      ellipsis_label.set_align(view::layout::AlignH::Left,
                               view::layout::AlignV::Top);
      ellipsis_label.set_ellipsis(true);
      ellipsis_label.render(frame, bottom);
    }
  }

  render::AnsiRenderer r{std::cout};
  r.render(frame);

  return 0;
}

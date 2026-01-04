// samples/basic/components_demo.cpp
//
// Component demo: shows FillView, BorderView, LabelView, and HBox/VBox.

#include <iostream>

#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/render/ansi/ansi_renderer.h"
#include "glyph/view/frame.h"
#include "glyph/view/view.h"

#include "glyph/view/components/border.h"
#include "glyph/view/components/box_layout.h"
#include "glyph/view/components/fill.h"
#include "glyph/view/components/label.h"
#include "glyph/view/layout/inset.h"
#include "glyph/view/layout/split.h"

namespace {

  using namespace glyph;

  class BorderedFillView final : public view::View {
  public:
    BorderedFillView(view::FillView fill, view::BorderView border)
        : fill_(fill), border_(border) {
    }

    void render(view::Frame &f, core::Rect area) const override {
      if (area.empty())
        return;

      fill_.render(f, area);
      border_.render(f, area);

      const auto inner =
          view::layout::inset_rect(area, view::layout::Insets::all(1));
      fill_.render(f, inner);
    }

  private:
    view::FillView   fill_;
    view::BorderView border_;
  };

  class RightPaneView final : public view::View {
  public:
    RightPaneView(view::FillView fill, view::BorderView border)
        : fill_(fill), border_(border) {
    }

    void render(view::Frame &f, core::Rect area) const override {
      if (area.empty())
        return;

      fill_.render(f, area);
      border_.render(f, area);

      view::layout::SplitRatio halves[] = {{1}, {1}};
      const auto rows = view::layout::layout_split_ratio(
          view::layout::Axis::Vertical, area, halves, 1);
      if (rows.rects.size() < 2) {
        return;
      }

      const auto top = view::layout::inset_rect(rows.rects[0],
                                                view::layout::Insets::all(1));
      const auto bottom = view::layout::inset_rect(
          rows.rects[1], view::layout::Insets::all(1));

      view::LabelView wrap_label(
          U"Wrap: The quick brown fox jumps over the lazy dog.");
      wrap_label.set_align(view::layout::AlignH::Left, view::layout::AlignV::Top);
      wrap_label.set_wrap(true);
      wrap_label.render(f, top);

      view::LabelView ellipsis_label(
          U"Ellipsis: The quick brown fox jumps over the lazy dog.");
      ellipsis_label.set_align(view::layout::AlignH::Left,
                               view::layout::AlignV::Top);
      ellipsis_label.set_ellipsis(true);
      ellipsis_label.render(f, bottom);
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

  view::FillView   fill_left(core::Cell::from_char(U'L'));
  view::FillView   fill_mid(core::Cell::from_char(U' '));
  view::BorderView border_mid(core::Cell::from_char(U'#'));
  BorderedFillView mid(fill_mid, border_mid);

  view::FillView   fill_right(core::Cell::from_char(U' '));
  view::BorderView border_right(core::Cell::from_char(U'='));
  RightPaneView    right(fill_right, border_right);

  auto layout = view::HBox({
      view::BoxChild{.view = &fill_left, .weight = 1},
      view::BoxChild{.view = &mid, .weight = 1},
      view::BoxChild{.view = &right, .weight = 1},
  }, 1);

  layout.render(frame, area);

  render::AnsiRenderer r{std::cout};
  r.render(frame);

  return 0;
}
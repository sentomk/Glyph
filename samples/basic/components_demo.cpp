// samples/basic/components_demo.cpp
//
// Component demo: shows FillView, BorderView, LabelView, and HStack/VStack.

#include <iostream>

#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/core/style.h"
#include "glyph/render/ansi/ansi_renderer.h"
#include "glyph/view/frame.h"
#include "glyph/view/view.h"

#include "glyph/view/components/panel.h"
#include "glyph/view/components/stack.h"
#include "glyph/view/components/inset.h"
#include "glyph/view/components/fill.h"
#include "glyph/view/components/label.h"
#include "glyph/view/layout/box.h"
#include "glyph/view/layout/inset.h"

int main() {
  using namespace glyph;

  // Step 1: Create a frame and give it a visible background.
  view::Frame frame{core::Size{60, 14}};
  frame.fill(core::Cell::from_char(U'.'));

  // Step 2: Define the content area with a uniform inset.
  const auto area =
      view::layout::inset_rect(frame.bounds(), view::layout::Insets::all(1));

  // Step 3: Build left column content (fill + title label styling).
  view::FillView  fill_left(core::Cell::from_char(U'L'));
  view::LabelView title(U"Glyph Components");
  title.set_align(view::layout::AlignH::Center, view::layout::AlignV::Top);
  {
    core::Style s = core::Style::with_fg(core::Style::rgb(255, 215, 0));
    s.attrs       = core::Style::AttrBold;
    title.set_cell(core::Cell::from_char(U' ', s));
  }
  // Step 4: Build center column content (panel with fill + border).
  view::FillView fill_mid(core::Cell::from_char(U' '));

  // Step 5: Build right column content (two labels with padding + layout).
  view::LabelView top(U"Wrap: The quick brown fox jumps over the lazy dog.");
  top.set_align(view::layout::AlignH::Left, view::layout::AlignV::Top);
  top.set_wrap_mode(view::LabelView::WrapMode::Word);

  view::InsetView top_pad(&top, view::layout::Insets::all(1));

  view::LabelView bottom(
      U"Ellipsis: The quick brown fox jumps over the lazy dog.");
  bottom.set_align(view::layout::AlignH::Left, view::layout::AlignV::Top);
  bottom.set_wrap(false);
  bottom.set_ellipsis(true);

  view::InsetView bottom_pad(&bottom, view::layout::Insets::all(1));

  auto right_content = view::VStack(
      {
          view::StackChild{.view = &top_pad, .weight = 1},
          view::StackChild{.view = &bottom_pad, .weight = 1},
      },
      1);

  // Step 6: Wrap center/right content in panels with their own styling.
  view::PanelView mid_panel(&fill_mid);
  mid_panel.set_fill(core::Cell::from_char(U' '));
  mid_panel.set_border(core::Cell::from_char(U'#'));
  mid_panel.set_padding(view::layout::Insets::all(1));
  mid_panel.set_draw_fill(true);
  mid_panel.set_draw_border(true);

  view::PanelView right_panel(&right_content);
  right_panel.set_fill(core::Cell::from_char(U' '));
  right_panel.set_border(core::Cell::from_char(U'='));
  right_panel.set_padding(view::layout::Insets::all(1));
  right_panel.set_draw_fill(true);
  right_panel.set_draw_border(true);

  // Step 7: Lay out three columns horizontally and render them.
  auto layout = view::HStack(
      {
          view::StackChild{.view = &fill_left, .weight = 1},
          view::StackChild{.view = &mid_panel, .weight = 1},
          view::StackChild{.view = &right_panel, .weight = 1},
      },
      1);

  layout.render(frame, area);

  // Step 8: Place the title inside the left column only.
  view::layout::BoxItem cols[] = {
      {-1, 1},
      {-1, 1},
      {-1, 1},
  };
  const auto col_rects =
      view::layout::layout_box(view::layout::Axis::Horizontal, area, cols, 1);

  const auto title_area = view::layout::inset_rect(
      col_rects.rects[0], view::layout::Insets::all(1));
  title.render(frame, title_area);

  // Step 9: Output the frame via the ANSI renderer.
  render::AnsiRenderer r{std::cout};
  r.render(frame);

  return 0;
}

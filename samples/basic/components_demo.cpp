// samples/basic/components_demo.cpp
//
// Component demo: shows FillView, BorderView, LabelView, and HStack/VStack.

#include <iostream>

#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/render/ansi/ansi_renderer.h"
#include "glyph/view/frame.h"
#include "glyph/view/view.h"

#include "glyph/view/components/panel.h"
#include "glyph/view/components/stack.h"
#include "glyph/view/components/fill.h"
#include "glyph/view/components/label.h"
#include "glyph/view/layout/inset.h"

namespace {

  using namespace glyph;

  class RightPaneView final : public view::View {
  public:
    void render(view::Frame &f, core::Rect area) const override {
      if (area.empty())
        return;

      class LabelPaneView final : public view::View {
      public:
        LabelPaneView(std::u32string text, bool wrap, bool ellipsis)
            : text_(std::move(text)), wrap_(wrap), ellipsis_(ellipsis) {
        }

        void render(view::Frame &f, core::Rect area) const override {
          if (area.empty())
            return;

          const auto inner =
              view::layout::inset_rect(area, view::layout::Insets::all(1));

          view::LabelView label(text_);
          label.set_align(
              view::layout::AlignH::Left, view::layout::AlignV::Top);
          if (wrap_) {
            label.set_wrap_mode(view::LabelView::WrapMode::Word);
          }
          else {
            label.set_wrap(false);
          }
          label.set_ellipsis(ellipsis_);
          label.render(f, inner);
        }

      private:
        std::u32string text_;
        bool           wrap_     = false;
        bool           ellipsis_ = false;
      };

      LabelPaneView top(
          U"Wrap: The quick brown fox jumps over the lazy dog.", true, false);
      LabelPaneView bottom(
          U"Ellipsis: The quick brown fox jumps over the lazy dog.",
          false,
          true);

      auto stack = view::VStack(
          {
              view::StackChild{.view = &top, .weight = 1},
              view::StackChild{.view = &bottom, .weight = 1},
          },
          1);
      stack.render(f, area);
    }
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
  RightPaneView    right_content;

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

  auto layout = view::HStack(
      {
          view::StackChild{.view = &fill_left, .weight = 1},
          view::StackChild{.view = &mid_panel, .weight = 1},
          view::StackChild{.view = &right_panel, .weight = 1},
      },
      1);

  layout.render(frame, area);

  render::AnsiRenderer r{std::cout};
  r.render(frame);

  return 0;
}

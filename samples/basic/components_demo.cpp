// samples/basic/components_demo.cpp
//
// Component demo: shows FillView + BorderView composition.

#include <iostream>

#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/render/ansi/ansi_renderer.h"
#include "glyph/view/frame.h"
#include "glyph/view/view.h"

#include "glyph/view/components/border.h"
#include "glyph/view/components/fill.h"
#include "glyph/view/layout/inset.h"

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

  view::Frame frame{core::Size{24, 8}};

  view::FillView   fill(core::Cell::from_char(U'.'));
  view::BorderView border(core::Cell::from_char(U'#'));

  BoxView box(fill, border);
  box.render(frame, frame.bounds());

  render::AnsiRenderer r{std::cout};
  r.render(frame);

  return 0;
}
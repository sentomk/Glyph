// Unit tests for PanelView border drawing and BorderStyle presets.

#include <doctest/doctest.h>

#include "glyph/core/geometry.h"
#include "glyph/view/components/panel.h"
#include "glyph/view/frame.h"

using namespace glyph;
using namespace glyph::view;

namespace {

  // Render a border style over a w x h area and return the frame.
  Frame bordered(const BorderStyle &style, int w, int h) {
    Frame    frame{core::Size{w, h}};
    PanelView      panel;
    panel.set_border_style(style);
    panel.render(frame, core::Rect{0, 0, w, h});
    return frame;
  }

} // namespace

TEST_CASE("BorderStyle: rounded places corners and edges") {
  const auto f = bordered(BorderStyle::rounded(), 5, 3);
  CHECK(f.view().at(0, 0).ch == U'╭');
  CHECK(f.view().at(4, 0).ch == U'╮');
  CHECK(f.view().at(0, 2).ch == U'╰');
  CHECK(f.view().at(4, 2).ch == U'╯');
  CHECK(f.view().at(1, 0).ch == U'─');
  CHECK(f.view().at(3, 2).ch == U'─');
  CHECK(f.view().at(0, 1).ch == U'│');
  CHECK(f.view().at(4, 1).ch == U'│');
  // Interior untouched.
  CHECK(f.view().at(2, 1).ch == U' ');
}

TEST_CASE("BorderStyle: square and double_line glyphs") {
  const auto sq = bordered(BorderStyle::square(), 3, 3);
  CHECK(sq.view().at(0, 0).ch == U'┌');
  CHECK(sq.view().at(2, 2).ch == U'┘');
  CHECK(sq.view().at(1, 1).ch == U' ');

  const auto db = bordered(BorderStyle::double_line(), 3, 3);
  CHECK(db.view().at(0, 0).ch == U'╔');
  CHECK(db.view().at(2, 0).ch == U'╗');
  CHECK(db.view().at(1, 0).ch == U'═');
  CHECK(db.view().at(0, 1).ch == U'║');
  CHECK(db.view().at(2, 2).ch == U'╝');
}

TEST_CASE("set_border_style enables the border on its own") {
  Frame frame{core::Size{3, 3}};
  PanelView   panel; // no set_border / set_draw_border call
  REQUIRE(frame.view().at(0, 0).ch == U' ');
  panel.set_border_style(BorderStyle::rounded());
  panel.render(frame, core::Rect{0, 0, 3, 3});
  CHECK(frame.view().at(0, 0).ch == U'╭');
}

TEST_CASE("set_border keeps uniform single-cell behavior") {
  Frame frame{core::Size{3, 3}};
  PanelView   panel;
  panel.set_border(core::Cell::from_char(U'#'));
  panel.render(frame, core::Rect{0, 0, 3, 3});
  CHECK(frame.view().at(0, 0).ch == U'#');
  CHECK(frame.view().at(2, 0).ch == U'#');
  CHECK(frame.view().at(1, 1).ch == U' ');
}

TEST_CASE("degenerate 1x1 area draws the bottom-right corner") {
  const auto f = bordered(BorderStyle::rounded(), 1, 1);
  CHECK(f.view().at(0, 0).ch == U'╯');
}

TEST_CASE("degenerate 3x1 row draws corners and horizontal edge") {
  const auto f = bordered(BorderStyle::square(), 3, 1);
  CHECK(f.view().at(0, 0).ch == U'└');
  CHECK(f.view().at(1, 0).ch == U'─');
  CHECK(f.view().at(2, 0).ch == U'┘');
}

TEST_CASE("BorderStyle::with_style recolors without reshaping") {
  core::Style   accent{};
  accent.fg(0xFF8800);
  const BorderStyle styled = BorderStyle::rounded().with_style(accent);
  CHECK(styled.top_left.ch == U'╭');
  CHECK(styled.top_left.style.fg_rgb == 0xFF8800);
  CHECK(styled.horizontal.ch == U'─');
  CHECK(styled.horizontal.style.fg_rgb == 0xFF8800);
}

TEST_CASE("PanelStyle preset still applies a uniform border") {
  Frame frame{core::Size{3, 3}};
  PanelView   panel;
  panel.set_style(PanelStyle::card(0x00FF00, U'+'));
  panel.render(frame, core::Rect{0, 0, 3, 3});
  CHECK(frame.view().at(0, 0).ch == U'+');
  CHECK(frame.view().at(2, 2).ch == U'+');
  CHECK(frame.view().at(0, 0).style.fg_rgb == 0x00FF00);
}

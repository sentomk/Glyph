// Integration tests for the View -> Frame -> Renderer pipeline.
//
// Renders real components into a Frame and asserts on the resulting cell
// grid, then sanity-checks that DebugRenderer reflects that grid.

#include <doctest/doctest.h>

#include <sstream>
#include <string>

#include "glyph/core/geometry.h"
#include "glyph/render/debug/debug_renderer.h"
#include "glyph/view/components/label.h"
#include "glyph/view/components/panel.h"
#include "glyph/view/frame.h"

using namespace glyph;

namespace {
  // Read one row of the frame as a UTF-32 string (skipping spacer cells).
  std::u32string row_text(const view::Frame &f, core::coord_t y) {
    std::u32string s;
    const auto      v = f.view();
    for (core::coord_t x = 0; x < f.size().w; ++x) {
      const auto &c = v.at(x, y);
      if (c.width == 0)
        continue;
      s.push_back(c.ch);
    }
    return s;
  }
} // namespace

TEST_CASE("LabelView renders text into the frame") {
  view::Frame frame{core::Size{10, 1}};
  frame.fill(core::Cell::from_char(U' '));

  view::LabelView label{U"Hi"};
  label.set_align(view::layout::AlignH::Left, view::layout::AlignV::Top);
  label.render(frame, frame.bounds());

  const auto row = row_text(frame, 0);
  CHECK(row.find(U"Hi") != std::u32string::npos);
}

TEST_CASE("LabelView right alignment places text at the end") {
  view::Frame frame{core::Size{6, 1}};
  frame.fill(core::Cell::from_char(U' '));

  view::LabelView label{U"ok"};
  label.set_align(view::layout::AlignH::Right, view::layout::AlignV::Top);
  label.render(frame, frame.bounds());

  // Last two visible cells should be 'o','k'.
  const auto v = frame.view();
  CHECK(v.at(4, 0).ch == U'o');
  CHECK(v.at(5, 0).ch == U'k');
}

TEST_CASE("rendering respects the given area (clipping)") {
  view::Frame frame{core::Size{10, 3}};
  frame.fill(core::Cell::from_char(U'.'));

  // Render a label only into the middle row, columns 2..6.
  view::LabelView label{U"XXXX"};
  label.render(frame, core::Rect{2, 1, 4, 1});

  const auto v = frame.view();
  // Untouched rows keep their fill.
  CHECK(v.at(0, 0).ch == U'.');
  CHECK(v.at(0, 2).ch == U'.');
  // Column 0 of the target row is outside the label area -> still fill.
  CHECK(v.at(0, 1).ch == U'.');
}

TEST_CASE("PanelView draws a border around its area when enabled") {
  view::Frame frame{core::Size{6, 4}};
  frame.fill(core::Cell::from_char(U' '));

  view::PanelView panel{};
  panel.set_border(core::Cell::from_char(U'+'));
  panel.set_draw_border(true);
  panel.render(frame, frame.bounds());

  // Corners should be the border glyph (border is opt-in, off by default).
  const auto v = frame.view();
  CHECK(v.at(0, 0).ch != U' ');
  CHECK(v.at(5, 0).ch != U' ');
  CHECK(v.at(0, 3).ch != U' ');
  CHECK(v.at(5, 3).ch != U' ');
}

TEST_CASE("DebugRenderer emits frame metadata and surface") {
  view::Frame frame{core::Size{3, 2}};
  frame.fill(core::Cell::from_char(U'#'));

  std::ostringstream os;
  render::DebugRenderer dbg{os};
  dbg.render(frame);

  const std::string out = os.str();
  CHECK(out.find("size: 3x2") != std::string::npos);
  CHECK(out.find("surface:") != std::string::npos);
  CHECK(out.find('#') != std::string::npos);
}

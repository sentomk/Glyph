// Unit tests for StatusLineView (inline single-line status).

#include <doctest/doctest.h>

#include "glyph/core/geometry.h"
#include "glyph/view/components/status_line.h"
#include "glyph/view/frame.h"

using namespace glyph;
using namespace glyph::view;

TEST_CASE("StatusLineView: renders segments sequentially with styles") {
  core::Style accent{};
  accent.fg(0x00AAFF);
  core::Style dim{};
  dim.attrs = core::Style::AttrDim;

  StatusLineView status;
  status.set_segments({
      {.text = "neko", .style = accent},
      {.text = " | ", .style = dim},
      {.text = "3 applied", .style = core::Style{}},
  });

  Frame frame{core::Size{24, 1}};
  status.render(frame, core::Rect{0, 0, 24, 1});

  CHECK(frame.view().at(0, 0).ch == U'n');
  CHECK(frame.view().at(0, 0).style.fg_rgb == 0x00AAFF);
  CHECK(frame.view().at(4, 0).ch == U' ');
  CHECK(frame.view().at(5, 0).ch == U'|');
  CHECK(frame.view().at(5, 0).style.attrs == core::Style::AttrDim);
  CHECK(frame.view().at(7, 0).ch == U'3');
}

TEST_CASE("StatusLineView: clips at the area width") {
  StatusLineView status;
  status.set_segments({
      {.text = "aaaa", .style = {}},
      {.text = "bbbb", .style = {}},
  });

  Frame frame{core::Size{10, 1}};
  status.render(frame, core::Rect{0, 0, 6, 1});

  CHECK(frame.view().at(0, 0).ch == U'a');
  CHECK(frame.view().at(5, 0).ch == U'b');
  CHECK(frame.view().at(6, 0).ch == U' '); // clipped outside area
}

TEST_CASE("StatusLineView: empty area is a no-op") {
  StatusLineView status;
  status.set_segments({{.text = "x", .style = {}}});

  Frame frame{core::Size{4, 1}};
  status.render(frame, core::Rect{0, 0, 0, 1});
  status.render(frame, core::Rect{0, 0, 4, 0});
  CHECK(frame.view().at(0, 0).ch == U' ');
}

TEST_CASE("StatusLineView: add_segment appends") {
  StatusLineView status;
  status.add_segment("one");
  status.add_segment("two");

  Frame frame{core::Size{10, 1}};
  status.render(frame, core::Rect{0, 0, 10, 1});
  CHECK(frame.view().at(0, 0).ch == U'o');
  CHECK(frame.view().at(3, 0).ch == U't');
}

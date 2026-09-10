// Unit tests for ScrollRegionView (bounded scrollback rendering).

#include <doctest/doctest.h>

#include <string>

#include "glyph/core/geometry.h"
#include "glyph/view/components/scroll_region.h"
#include "glyph/view/frame.h"

using namespace glyph;
using namespace glyph::view;

TEST_CASE("ScrollRegionView: auto-scrolls to the newest lines") {
  ScrollRegionView logs;
  for (int i = 1; i <= 5; ++i) {
    logs.push_line("line" + std::to_string(i));
  }

  Frame frame{core::Size{10, 3}};
  logs.render(frame, core::Rect{0, 0, 10, 3});

  // Rows show line3 / line4 / line5; the digit is the 5th column.
  CHECK(frame.view().at(4, 0).ch == U'3');
  CHECK(frame.view().at(4, 1).ch == U'4');
  CHECK(frame.view().at(4, 2).ch == U'5');
}

TEST_CASE("ScrollRegionView: fewer lines than rows renders from the top") {
  ScrollRegionView logs;
  logs.push_line("only");

  Frame frame{core::Size{10, 4}};
  logs.render(frame, core::Rect{0, 0, 10, 4});

  CHECK(frame.view().at(0, 0).ch == U'o');
  CHECK(frame.view().at(0, 3).ch == U' '); // untouched row
}

TEST_CASE("ScrollRegionView: ring buffer drops the oldest lines") {
  ScrollRegionView logs{3};
  for (int i = 1; i <= 5; ++i) {
    logs.push_line("l" + std::to_string(i));
  }
  REQUIRE(logs.line_count() == 3);

  Frame frame{core::Size{10, 3}};
  logs.render(frame, core::Rect{0, 0, 10, 3});
  // Newest three: l3, l4, l5.
  CHECK(frame.view().at(1, 0).ch == U'3');
  CHECK(frame.view().at(1, 2).ch == U'5');
}

TEST_CASE("ScrollRegionView: scroll_up shows older lines") {
  ScrollRegionView logs;
  for (int i = 1; i <= 5; ++i) {
    logs.push_line("line" + std::to_string(i));
  }

  Frame frame{core::Size{10, 3}};
  logs.scroll_up(1);
  REQUIRE(logs.scroll_offset() == 1);
  logs.render(frame, core::Rect{0, 0, 10, 3});

  CHECK(frame.view().at(4, 0).ch == U'2');
  CHECK(frame.view().at(4, 2).ch == U'4');
}

TEST_CASE("ScrollRegionView: scroll_up past the oldest clamps") {
  ScrollRegionView logs;
  for (int i = 1; i <= 5; ++i) {
    logs.push_line("line" + std::to_string(i));
  }
  logs.scroll_up(100);

  Frame frame{core::Size{10, 3}};
  logs.render(frame, core::Rect{0, 0, 10, 3});
  CHECK(frame.view().at(4, 0).ch == U'1');
  CHECK(frame.view().at(4, 2).ch == U'3');
}

TEST_CASE("ScrollRegionView: push_line re-follows the bottom") {
  ScrollRegionView logs;
  for (int i = 1; i <= 5; ++i) {
    logs.push_line("line" + std::to_string(i));
  }
  logs.scroll_up(2);
  logs.push_line("line6");
  REQUIRE(logs.scroll_offset() == 0);

  Frame frame{core::Size{10, 3}};
  logs.render(frame, core::Rect{0, 0, 10, 3});
  CHECK(frame.view().at(4, 2).ch == U'6');
}

TEST_CASE("ScrollRegionView: per-line style is applied") {
  ScrollRegionView logs;
  core::Style err{};
  err.fg(0xFF0000);
  logs.push_line("ok");
  logs.push_line("bad", err);

  Frame frame{core::Size{10, 2}};
  logs.render(frame, core::Rect{0, 0, 10, 2});

  CHECK(frame.view().at(0, 1).style.fg_rgb == 0xFF0000);
  CHECK((frame.view().at(0, 0).style.flags & core::Style::FlagFgDefault) != 0);
}

TEST_CASE("ScrollRegionView: overflow wraps, bottom row of a 1-row view") {
  ScrollRegionView logs;
  logs.push_line("0123456789AB");

  Frame frame{core::Size{20, 1}};
  logs.render(frame, core::Rect{2, 0, 8, 1}); // columns 2..9

  // Width 8 wraps into "01234567" + "89AB"; the auto-followed window
  // shows the last visual row.
  CHECK(frame.view().at(2, 0).ch == U'8');
  CHECK(frame.view().at(5, 0).ch == U'B');
  // Column 6 is past the wrapped row and outside the area: untouched.
  CHECK(frame.view().at(6, 0).ch == U' ');
}

TEST_CASE("ScrollRegionView: long lines wrap onto visual rows") {
  ScrollRegionView logs;
  logs.push_line("abcdefgh"); // width 5 -> "abcde" + "fgh"

  Frame frame{core::Size{8, 2}};
  logs.render(frame, core::Rect{0, 0, 5, 2});
  CHECK(frame.view().at(0, 0).ch == U'a');
  CHECK(frame.view().at(4, 0).ch == U'e');
  CHECK(frame.view().at(0, 1).ch == U'f');
  CHECK(frame.view().at(2, 1).ch == U'h');
  CHECK(frame.view().at(3, 1).ch == U' '); // row 2 is shorter
}

TEST_CASE("ScrollRegionView: newlines split into rows") {
  ScrollRegionView logs;
  logs.push_line("a\n\nb"); // three rows: "a", "", "b"

  Frame frame{core::Size{4, 3}};
  logs.render(frame, core::Rect{0, 0, 4, 3});
  CHECK(frame.view().at(0, 0).ch == U'a');
  CHECK(frame.view().at(0, 1).ch == U' '); // blank row kept
  CHECK(frame.view().at(0, 2).ch == U'b');
}

TEST_CASE("ScrollRegionView: wrapping never splits wide glyphs") {
  ScrollRegionView logs;
  logs.push_line("中文abc"); // width 4 -> "中文" (4 cols) + "abc"

  Frame frame{core::Size{6, 2}};
  logs.render(frame, core::Rect{0, 0, 4, 2});
  CHECK(frame.view().at(0, 0).ch == U'中');
  CHECK(frame.view().at(2, 0).ch == U'文');
  CHECK(frame.view().at(0, 1).ch == U'a');
}

TEST_CASE("ScrollRegionView: scroll offset counts visual rows") {
  ScrollRegionView logs;
  logs.push_line("0123456789"); // width 5 -> rows "01234" "56789"
  logs.push_line("tail");

  Frame frame{core::Size{6, 2}};
  // Auto-follow: last two visual rows = "56789" + "tail".
  logs.render(frame, core::Rect{0, 0, 5, 2});
  CHECK(frame.view().at(0, 0).ch == U'5');
  CHECK(frame.view().at(0, 1).ch == U't');

  // One visual row up: "01234" + "56789".
  logs.scroll_up(1);
  logs.render(frame, core::Rect{0, 0, 5, 2});
  CHECK(frame.view().at(0, 0).ch == U'0');
  CHECK(frame.view().at(0, 1).ch == U'5');
}

TEST_CASE("ScrollRegionView: on_mouse wires the wheel (issue 5)") {
  ScrollRegionView logs;
  for (int i = 1; i <= 5; ++i) {
    logs.push_line("line" + std::to_string(i));
  }

  core::MouseEvent wheel_up{};
  wheel_up.action = core::MouseAction::Scroll;
  wheel_up.button = core::MouseButton::WheelUp;
  logs.on_mouse(wheel_up);
  CHECK(logs.scroll_offset() == 3); // default 3 rows per notch

  core::MouseEvent wheel_down{};
  wheel_down.action = core::MouseAction::Scroll;
  wheel_down.button = core::MouseButton::WheelDown;
  logs.on_mouse(wheel_down);
  CHECK(logs.scroll_offset() == 0);

  // Non-scroll events are ignored.
  core::MouseEvent press{};
  press.action = core::MouseAction::Down;
  press.button = core::MouseButton::WheelUp;
  logs.on_mouse(press);
  CHECK(logs.scroll_offset() == 0);

  // Custom step; zero clamps to one.
  logs.set_wheel_lines(1);
  logs.on_mouse(wheel_up);
  CHECK(logs.scroll_offset() == 1);
  logs.set_wheel_lines(0);
  logs.on_mouse(wheel_up);
  CHECK(logs.scroll_offset() == 2);
}

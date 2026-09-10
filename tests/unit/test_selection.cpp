// Unit tests for SelectionModel (rect tracking, highlight, extraction).

#include <doctest/doctest.h>

#include <string>

#include "glyph/core/geometry.h"
#include "glyph/view/frame.h"
#include "glyph/view/selection.h"
#include "glyph/view/text.h"

using namespace glyph;
using namespace glyph::view;

TEST_CASE("SelectionModel: rect is normalized regardless of drag direction") {
  SelectionModel sel;
  CHECK_FALSE(sel.active());

  sel.begin({5, 3});
  CHECK(sel.active());
  CHECK(sel.rect() == core::Rect{core::Point{5, 3}, core::Size{1, 1}});

  sel.extend({1, 1});
  CHECK(sel.rect() == core::Rect{core::Point{1, 1}, core::Size{5, 3}});
}

TEST_CASE("SelectionModel: clear deactivates") {
  SelectionModel sel;
  sel.begin({0, 0});
  sel.extend({2, 0});
  sel.clear();
  CHECK_FALSE(sel.active());

  Frame frame{core::Size{8, 1}};
  CHECK(sel.extract(frame).empty());
}

TEST_CASE("SelectionModel: highlight applies reverse video in-rect only") {
  Frame          frame{core::Size{6, 2}};
  SelectionModel sel;
  sel.begin({1, 0});
  sel.extend({3, 1});
  sel.highlight(frame);

  CHECK((frame.at(1, 0).style.attrs & core::Style::AttrReverse) != 0);
  CHECK((frame.at(3, 1).style.attrs & core::Style::AttrReverse) != 0);
  CHECK((frame.at(0, 0).style.attrs & core::Style::AttrReverse) == 0);
  CHECK((frame.at(4, 0).style.attrs & core::Style::AttrReverse) == 0);
}

TEST_CASE("SelectionModel: extract single row, no trailing newline") {
  Frame frame{core::Size{16, 1}};
  draw_text(frame, {0, 0}, "hello world");

  SelectionModel sel;
  sel.begin({0, 0});
  sel.extend({4, 0});
  CHECK(sel.extract(frame) == "hello");
}

TEST_CASE("SelectionModel: extract joins rows and trims padding") {
  Frame frame{core::Size{12, 3}};
  draw_text(frame, {0, 0}, "alpha");
  draw_text(frame, {0, 1}, "beta");
  draw_text(frame, {0, 2}, "gamma");

  SelectionModel sel;
  sel.begin({0, 0});
  sel.extend({11, 1}); // two full rows
  CHECK(sel.extract(frame) == "alpha\nbeta");
}

TEST_CASE("SelectionModel: wide glyphs extract once, spacers are skipped") {
  Frame frame{core::Size{8, 1}};
  draw_text(frame, {0, 0}, "中a");

  SelectionModel sel;
  sel.begin({0, 0});
  sel.extend({2, 0});
  const std::string got = sel.extract(frame);
  CHECK(got == "中a"); // 4 bytes + 'a', no spacer column
}

TEST_CASE("SelectionModel: rect is clamped to the frame") {
  Frame frame{core::Size{4, 1}};
  draw_text(frame, {0, 0}, "abcd");

  SelectionModel sel;
  sel.begin({-2, -2});
  sel.extend({9, 9});
  CHECK(sel.extract(frame) == "abcd");
}

TEST_CASE("SelectionModel: highlight preserves wide glyphs") {
  // Regression: Buffer::put treated a spacer write-back as new content
  // and erased the wide glyph to its left, so highlighting a CJK row
  // blanked it — on screen and in extract().
  Frame frame{core::Size{10, 1}};
  draw_text(frame, {0, 0}, "中文a");

  SelectionModel sel;
  sel.begin({0, 0});
  sel.extend({4, 0});
  sel.highlight(frame);

  CHECK((frame.at(0, 0).style.attrs & core::Style::AttrReverse) != 0);
  CHECK(frame.at(0, 0).ch == U'中');
  CHECK(frame.at(2, 0).ch == U'文');
  CHECK(sel.extract(frame) == "中文a");
}

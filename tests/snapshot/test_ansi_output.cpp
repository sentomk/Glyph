// Snapshot tests for the ANSI renderer output stream.
//
// Asserts on structural fragments of the emitted escape sequences rather
// than exact byte-for-byte output, so harmless formatting tweaks do not
// break the suite. Covers the first-frame full redraw and the subsequent
// dirty/diff incremental path.

#include <doctest/doctest.h>

#include <sstream>
#include <string>

#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/render/ansi/ansi_renderer.h"
#include "glyph/view/frame.h"

using namespace glyph;

namespace {
  bool contains(const std::string &hay, const std::string &needle) {
    return hay.find(needle) != std::string::npos;
  }
} // namespace

TEST_CASE("first frame performs a full redraw") {
  std::ostringstream os;
  render::AnsiRenderer r{os};

  view::Frame frame{core::Size{4, 2}};
  frame.fill(core::Cell::from_char(U'A'));
  r.render(frame);

  const std::string out = os.str();
  // Home cursor + reset style at the start of a full redraw.
  CHECK(contains(out, "\x1b[H"));
  CHECK(contains(out, "\x1b[0m"));
  // The filled glyph must appear in the output.
  CHECK(contains(out, "A"));
}

TEST_CASE("an unchanged second frame emits no cell content") {
  std::ostringstream os;
  render::AnsiRenderer r{os};

  view::Frame frame{core::Size{4, 2}};
  frame.fill(core::Cell::from_char(U'A'));
  r.render(frame);

  const std::size_t after_first = os.str().size();

  // Re-render an identical, freshly built frame: no dirty lines beyond
  // construction, content identical -> minimal or no output.
  view::Frame frame2{core::Size{4, 2}};
  frame2.fill(core::Cell::from_char(U'A'));
  // Drain construction dirty so only real changes count.
  (void)frame2.take_dirty_lines();
  r.render(frame2);

  const std::string second = os.str().substr(after_first);
  CHECK_FALSE(contains(second, "A")); // no glyph re-emitted
}

TEST_CASE("incremental frame redraws only the changed cell") {
  std::ostringstream os;
  render::AnsiRenderer r{os};

  view::Frame frame{core::Size{5, 3}};
  frame.fill(core::Cell::from_char(U'.'));
  r.render(frame);

  const std::size_t after_first = os.str().size();

  // Change a single cell on row 1.
  view::Frame frame2{core::Size{5, 3}};
  frame2.fill(core::Cell::from_char(U'.'));
  (void)frame2.take_dirty_lines();
  frame2.set(core::Point{2, 1}, core::Cell::from_char(U'Z'));
  r.render(frame2);

  const std::string inc = os.str().substr(after_first);
  // The new glyph is emitted...
  CHECK(contains(inc, "Z"));
  // ...positioned on row 2 (1-based) via a cursor move to that row.
  CHECK(contains(inc, ";2H") == false); // not a column-2-row form
  CHECK(contains(inc, "2;")); // row 2 appears in some "row;col H" move
}

TEST_CASE("reset() forces a full redraw on the next frame") {
  std::ostringstream os;
  render::AnsiRenderer r{os};

  view::Frame frame{core::Size{3, 1}};
  frame.fill(core::Cell::from_char(U'Q'));
  r.render(frame);

  r.reset();

  const std::size_t mark = os.str().size();
  view::Frame frame2{core::Size{3, 1}};
  frame2.fill(core::Cell::from_char(U'Q'));
  r.render(frame2);

  const std::string after_reset = os.str().substr(mark);
  // Full redraw again: glyph re-emitted despite identical content.
  CHECK(contains(after_reset, "Q"));
}

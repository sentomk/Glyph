// Unit tests for core::Buffer / BufferView (write, clip, blit, dirty).

#include <doctest/doctest.h>

#include "glyph/core/buffer.h"

using namespace glyph::core;

TEST_CASE("Buffer construction and bounds") {
  Buffer b{Size{4, 3}};
  CHECK(b.size() == Size{4, 3});
  CHECK_FALSE(b.empty());
  CHECK(Buffer{}.empty());
  CHECK(Buffer{Size{0, 5}}.empty());
}

TEST_CASE("put writes a cell at the target") {
  Buffer b{Size{4, 3}};
  b.view().put(Point{1, 1}, Cell::from_char(U'X'));
  CHECK(b.const_view().at(1, 1).ch == U'X');
  CHECK(b.const_view().at(0, 0).ch == U' '); // default fill
}

TEST_CASE("put ignores out-of-bounds coordinates") {
  Buffer b{Size{2, 2}};
  b.view().put(Point{-1, 0}, Cell::from_char(U'X'));
  b.view().put(Point{0, 5}, Cell::from_char(U'Y'));
  b.view().put(Point{2, 0}, Cell::from_char(U'Z')); // x == width
  // Nothing crashed; interior remains default.
  CHECK(b.const_view().at(0, 0).ch == U' ');
}

TEST_CASE("put places a wide glyph with a zero-width spacer") {
  Buffer b{Size{4, 1}};
  b.view().put(Point{0, 0}, Cell::from_char(U'中'));
  CHECK(b.const_view().at(0, 0).ch == U'中');
  CHECK(b.const_view().at(0, 0).width == 2);
  // Trailing spacer cell.
  CHECK(b.const_view().at(1, 0).width == 0);
}

TEST_CASE("wide glyph at last column degrades to width 1") {
  Buffer b{Size{2, 1}};
  b.view().put(Point{1, 0}, Cell::from_char(U'中')); // no room for spacer
  CHECK(b.const_view().at(1, 0).width == 1);
}

TEST_CASE("overwriting a wide lead clears its spacer") {
  Buffer b{Size{4, 1}};
  auto v = b.view();
  v.put(Point{0, 0}, Cell::from_char(U'中'));
  v.put(Point{0, 0}, Cell::from_char(U'A')); // overwrite lead
  CHECK(b.const_view().at(0, 0).ch == U'A');
  CHECK(b.const_view().at(1, 0) == Cell{}); // spacer cleared
}

TEST_CASE("fill_rect clips to bounds") {
  Buffer b{Size{3, 3}};
  b.view().fill_rect(Rect{1, 1, 10, 10}, Cell::from_char(U'#'));
  CHECK(b.const_view().at(0, 0).ch == U' ');
  CHECK(b.const_view().at(1, 1).ch == U'#');
  CHECK(b.const_view().at(2, 2).ch == U'#');
}

TEST_CASE("blit copies a source view, clipped") {
  Buffer src{Size{2, 2}};
  src.view().clear(Cell::from_char(U'*'));

  Buffer dst{Size{4, 4}};
  dst.view().blit(src.const_view(), Point{1, 1});
  CHECK(dst.const_view().at(1, 1).ch == U'*');
  CHECK(dst.const_view().at(2, 2).ch == U'*');
  CHECK(dst.const_view().at(0, 0).ch == U' ');
  CHECK(dst.const_view().at(3, 3).ch == U' ');
}

TEST_CASE("dirty lines track writes") {
  Buffer b{Size{4, 4}};
  (void)b.take_dirty_lines(); // drain construction state

  b.view().put(Point{0, 2}, Cell::from_char(U'A'));
  auto dirty = b.take_dirty_lines();
  REQUIRE(dirty.size() == 1);
  CHECK(dirty[0] == 2);

  // After taking, dirty is cleared.
  CHECK(b.take_dirty_lines().empty());
}

TEST_CASE("clear marks all lines dirty") {
  Buffer b{Size{2, 3}};
  (void)b.take_dirty_lines();
  b.view().clear(Cell::from_char(U'.'));
  auto dirty = b.take_dirty_lines();
  CHECK(dirty.size() == 3);
}

TEST_CASE("resize preserves overlapping region") {
  Buffer b{Size{2, 2}};
  b.view().put(Point{0, 0}, Cell::from_char(U'A'));
  b.resize(Size{4, 4});
  CHECK(b.size() == Size{4, 4});
  CHECK(b.const_view().at(0, 0).ch == U'A');
}

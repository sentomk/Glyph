// Unit tests for buffer diffing (diff_lines / diff_spans / hash_line).

#include <doctest/doctest.h>

#include "glyph/core/buffer.h"
#include "glyph/core/diff.h"

using namespace glyph::core;

namespace {
  Buffer make(Size s, char32_t fill = U' ') {
    Buffer b{s};
    b.view().clear(Cell::from_char(fill));
    return b;
  }
} // namespace

TEST_CASE("diff_lines: identical buffers report no change") {
  Buffer a = make(Size{4, 3});
  Buffer b = make(Size{4, 3});
  CHECK(diff_lines(a.const_view(), b.const_view()).empty());
}

TEST_CASE("diff_lines: a single changed line") {
  Buffer a = make(Size{4, 3});
  Buffer b = make(Size{4, 3});
  b.view().put(Point{2, 1}, Cell::from_char(U'X'));

  auto dirty = diff_lines(a.const_view(), b.const_view());
  REQUIRE(dirty.size() == 1);
  CHECK(dirty[0] == 1);
}

TEST_CASE("diff_lines: differing sizes mark all next lines dirty") {
  Buffer a = make(Size{4, 2});
  Buffer b = make(Size{5, 3});
  auto dirty = diff_lines(a.const_view(), b.const_view());
  CHECK(dirty.size() == 3); // all rows of next
}

TEST_CASE("diff_spans: minimal changed interval on a line") {
  Buffer a = make(Size{6, 1});
  Buffer b = make(Size{6, 1});
  b.view().put(Point{2, 0}, Cell::from_char(U'A'));
  b.view().put(Point{3, 0}, Cell::from_char(U'B'));

  auto spans = diff_spans(a.const_view(), b.const_view());
  REQUIRE(spans.size() == 1);
  CHECK(spans[0].y == 0);
  CHECK(spans[0].x0 == 2);
  CHECK(spans[0].x1 == 4); // half-open: covers cols 2 and 3
}

TEST_CASE("diff_spans: two disjoint changes on one line yield two spans") {
  Buffer a = make(Size{8, 1});
  Buffer b = make(Size{8, 1});
  b.view().put(Point{1, 0}, Cell::from_char(U'A'));
  b.view().put(Point{5, 0}, Cell::from_char(U'B'));

  auto spans = diff_spans(a.const_view(), b.const_view());
  REQUIRE(spans.size() == 2);
  CHECK(spans[0].x0 == 1);
  CHECK(spans[0].x1 == 2);
  CHECK(spans[1].x0 == 5);
  CHECK(spans[1].x1 == 6);
}

TEST_CASE("hash_line: equal content hashes equal, different content differs") {
  Buffer a = make(Size{4, 1});
  Buffer b = make(Size{4, 1});
  CHECK(hash_line(a.const_view(), 0) == hash_line(b.const_view(), 0));

  b.view().put(Point{0, 0}, Cell::from_char(U'Z'));
  CHECK(hash_line(a.const_view(), 0) != hash_line(b.const_view(), 0));
}

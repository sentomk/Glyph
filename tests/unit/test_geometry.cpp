// Unit tests for core geometry primitives (Point / Size / Rect).

#include <doctest/doctest.h>

#include "glyph/core/geometry.h"

using namespace glyph::core;

TEST_CASE("Point arithmetic and equality") {
  Point a{2, 3};
  Point b{4, 1};
  CHECK(a + b == Point{6, 4});
  CHECK(b - a == Point{2, -2});
  CHECK(a != b);

  Point c = a;
  c += Point{1, 1};
  CHECK(c == Point{3, 4});
}

TEST_CASE("Size empty semantics") {
  CHECK(Size{0, 5}.empty());
  CHECK(Size{5, 0}.empty());
  CHECK(Size{-1, 3}.empty());
  CHECK_FALSE(Size{1, 1}.empty());
  CHECK(Size{-2, -3}.non_negative() == Size{0, 0});
}

TEST_CASE("Rect half-open edges") {
  Rect r{Point{2, 3}, Size{4, 5}};
  CHECK(r.left() == 2);
  CHECK(r.top() == 3);
  CHECK(r.right() == 6);    // 2 + 4
  CHECK(r.bottom() == 8);   // 3 + 5
  CHECK_FALSE(r.empty());
  CHECK(Rect{0, 0, 0, 4}.empty());
}

TEST_CASE("Rect::contains uses half-open intervals") {
  Rect r{0, 0, 3, 3};
  CHECK(r.contains(Point{0, 0}));
  CHECK(r.contains(Point{2, 2}));
  CHECK_FALSE(r.contains(Point{3, 0})); // right edge excluded
  CHECK_FALSE(r.contains(Point{0, 3})); // bottom edge excluded
  CHECK_FALSE(r.contains(Point{-1, 0}));
  // Empty rect contains nothing.
  CHECK_FALSE(Rect{0, 0, 0, 0}.contains(Point{0, 0}));
}

TEST_CASE("Rect::intersect") {
  Rect a{0, 0, 4, 4};
  Rect b{2, 2, 4, 4};
  Rect i = a.intersect(b);
  CHECK(i.origin == Point{2, 2});
  CHECK(i.size == Size{2, 2});

  // Disjoint -> empty.
  Rect d = a.intersect(Rect{10, 10, 2, 2});
  CHECK(d.empty());

  // Intersect with empty -> empty.
  CHECK(a.intersect(Rect{0, 0, 0, 0}).empty());
}

TEST_CASE("Rect::unite handles empties") {
  Rect a{0, 0, 2, 2};
  Rect b{4, 4, 2, 2};
  Rect u = a.unite(b);
  CHECK(u.origin == Point{0, 0});
  CHECK(u.right() == 6);
  CHECK(u.bottom() == 6);

  // Union with empty returns the non-empty operand.
  CHECK(a.unite(Rect{0, 0, 0, 0}) == a);
  CHECK(Rect{0, 0, 0, 0}.unite(a) == a);
}

TEST_CASE("Rect::translated") {
  Rect r{1, 1, 2, 2};
  CHECK(r.translated(Point{3, 4}).origin == Point{4, 5});
  CHECK(r.translated(Point{3, 4}).size == Size{2, 2});
}

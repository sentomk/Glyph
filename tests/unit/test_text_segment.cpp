// Unit tests for grapheme-cluster segmentation and cluster-aware UTF-8
// drawing (texere-backed UAX #29).

#include <doctest/doctest.h>

#include <string>
#include <utility>
#include <vector>

#include "glyph/core/text.h"
#include "glyph/view/frame.h"
#include "glyph/view/text.h"

using namespace glyph;
using namespace glyph::core;

namespace {

  // Walk utf8 and collect (base, width) per grapheme cluster.
  std::vector<std::pair<char32_t, std::uint8_t>> clusters(std::string_view s) {
    std::vector<std::pair<char32_t, std::uint8_t>> out;
    std::size_t pos = 0;
    while (pos < s.size()) {
      const Grapheme g = next_grapheme(s, pos);
      REQUIRE(g.next > pos); // always advances
      pos = g.next;
      if (g.width == 0) {
        continue; // pure zero-width cluster, no cell
      }
      out.emplace_back(g.base, g.width);
    }
    return out;
  }

} // namespace

TEST_CASE("next_grapheme: basic segmentation") {
  const std::string ascii = "hi";
  auto              cs    = clusters(ascii);
  REQUIRE(cs.size() == 2);
  CHECK(cs[0].first == U'h');
  CHECK(cs[1].first == U'i');
}

TEST_CASE("next_grapheme: VS16 sequence is one cluster, base width") {
  // U+2764 U+FE0F — one cluster; we store the base heart, width 1.
  const std::string heart = "\xE2\x9D\xA4\xEF\xB8\x8F";
  auto              cs    = clusters(heart);
  REQUIRE(cs.size() == 1);
  CHECK(cs[0].first == 0x2764);
  CHECK(cs[0].second == 1);
}

TEST_CASE("next_grapheme: ZWJ emoji family collapses to base") {
  // man ZWJ woman ZWJ girl — one cluster, stored as the man, width 2.
  const std::string family =
      "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7";
  auto cs = clusters(family);
  REQUIRE(cs.size() == 1);
  CHECK(cs[0].first == 0x1F468);
  CHECK(cs[0].second == 2);
}

TEST_CASE("next_grapheme: flag pair is one cluster, narrow base") {
  // U+1F1EF U+1F1F5 (JP) — one cluster; lone-indicator base is width 1.
  const std::string flag = "\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5";
  auto              cs   = clusters(flag);
  REQUIRE(cs.size() == 1);
  CHECK(cs[0].first == 0x1F1EF);
  CHECK(cs[0].second == 1);
}

TEST_CASE("next_grapheme: skin tone attaches to its base") {
  // thumbs up + medium skin tone — one cluster, width 2.
  const std::string toned = "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD";
  auto              cs    = clusters(toned);
  REQUIRE(cs.size() == 1);
  CHECK(cs[0].first == 0x1F44D);
  CHECK(cs[0].second == 2);
}

TEST_CASE("next_grapheme: combining mark attaches to its base") {
  // e + combining acute accent — one cluster.
  const std::string e_acute = "\x65\xCC\x81";
  auto              cs      = clusters(e_acute);
  REQUIRE(cs.size() == 1);
  CHECK(cs[0].first == U'e');
  CHECK(cs[0].second == 1);
}

TEST_CASE("next_grapheme: CJK stays wide and advances two columns") {
  const std::string cjk = "\xE4\xB8\xAD";
  auto              cs  = clusters(cjk);
  REQUIRE(cs.size() == 1);
  CHECK(cs[0].first == U'中');
  CHECK(cs[0].second == 2);
}

TEST_CASE("next_grapheme: mixed run keeps columns aligned") {
  // a | heart+VS16 | 中 | family | x
  const std::string mixed =
      "a"
      "\xE2\x9D\xA4\xEF\xB8\x8F"
      "\xE4\xB8\xAD"
      "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9"
      "x";
  auto cs = clusters(mixed);
  REQUIRE(cs.size() == 5);
  CHECK(cs[0] == std::make_pair(char32_t(U'a'), std::uint8_t(1)));
  CHECK(cs[1] == std::make_pair(char32_t(0x2764), std::uint8_t(1)));
  CHECK(cs[2] == std::make_pair(char32_t(U'中'), std::uint8_t(2)));
  CHECK(cs[3] == std::make_pair(char32_t(0x1F468), std::uint8_t(2)));
  CHECK(cs[4] == std::make_pair(char32_t(U'x'), std::uint8_t(1)));
}

TEST_CASE("next_grapheme: invalid byte yields replacement and advances") {
  const std::string bad = "\xFFz";
  std::size_t      pos = 0;
  const Grapheme   g1  = next_grapheme(bad, pos);
  CHECK(g1.base == U'\uFFFD');
  CHECK(g1.next == 1);
  const Grapheme g2 = next_grapheme(bad, g1.next);
  CHECK(g2.base == U'z');
  CHECK(g2.next == 2);
}

TEST_CASE("draw_text: cluster-aware column layout") {
  view::Frame frame{Size{16, 1}};
  draw_text(frame, Point{0, 0}, "a中b");

  CHECK(frame.view().at(0, 0).ch == U'a');
  CHECK(frame.view().at(1, 0).ch == U'中');
  CHECK(frame.view().at(1, 0).width == 2);
  // Buffer::put writes the zero-width spacer automatically.
  CHECK(frame.view().at(2, 0).width == 0);
  CHECK(frame.view().at(3, 0).ch == U'b');
}

TEST_CASE("draw_text: emoji sequence does not shift following columns") {
  view::Frame frame{Size{20, 1}};
  // family cluster (base width 2) followed by 'x' — x lands at column 2.
  draw_text(frame, Point{0, 0},
            "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9"
            "x");
  CHECK(frame.view().at(0, 0).ch == 0x1F468);
  CHECK(frame.view().at(0, 0).width == 2);
  CHECK(frame.view().at(2, 0).ch == U'x');
}

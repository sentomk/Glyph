// Unit tests for the codepoint width rule and Cell construction.

#include <doctest/doctest.h>

#include "glyph/core/cell.h"
#include "glyph/core/text.h"

using namespace glyph::core;

TEST_CASE("cell_width: control and null are zero-width") {
  CHECK(cell_width(U'\0') == 0);
  CHECK(cell_width(U'\x01') == 0);
  CHECK(cell_width(U'\x1b') == 0); // ESC
  CHECK(cell_width(0x7F) == 0);    // DEL
}

TEST_CASE("cell_width: ASCII and Latin are narrow") {
  CHECK(cell_width(U'A') == 1);
  CHECK(cell_width(U' ') == 1);
  CHECK(cell_width(U'~') == 1);
  CHECK(cell_width(U'é') == 1);
}

TEST_CASE("cell_width: CJK and fullwidth are wide") {
  CHECK(cell_width(U'中') == 2);  // CJK unified
  CHECK(cell_width(U'あ') == 2);  // Hiragana (0x3042, within 2E80..A4CF)
  CHECK(cell_width(U'한') == 2);  // Hangul syllable (AC00..D7A3)
  CHECK(cell_width(U'Ａ') == 2);  // Fullwidth Latin A (FF21, within FF00..FF60)
}

TEST_CASE("cell_width: Hangul Jamo range") {
  CHECK(cell_width(0x1100) == 2);
  CHECK(cell_width(0x115F) == 2);
}

TEST_CASE("cell_width: boundary codepoints") {
  // Just below CJK start is narrow.
  CHECK(cell_width(0x10FF) == 1);
  // Start of CJK-ish wide block.
  CHECK(cell_width(0x2E80) == 2);
  // Just past the FF00..FF60 wide band is narrow again.
  CHECK(cell_width(0xFF61) == 1);
}

TEST_CASE("cell_width: standalone emoji are wide") {
  CHECK(cell_width(U'\U0001F600') == 2); // grinning face 😀
  CHECK(cell_width(U'\U0001F44D') == 2); // thumbs up 👍
  CHECK(cell_width(U'\U0001F680') == 2); // rocket 🚀
  CHECK(cell_width(U'\U0001F9FF') == 2); // nazar amulet (end of ext range)
  CHECK(cell_width(U'\U0001F1E8') == 2); // regional indicator C (flag half)
}

TEST_CASE("cell_width: emoji range boundaries") {
  CHECK(cell_width(0x1F2FF) == 2); // last enclosed-ideographic
  CHECK(cell_width(0x1F300) == 2); // first pictograph
  CHECK(cell_width(0x1FAFF) == 2); // last of ext-A coverage
  CHECK(cell_width(0x1FB00) == 1); // just past emoji coverage -> narrow
}

TEST_CASE("cell_width: text-presentation symbols stay narrow (phase 1)") {
  // These are narrow by default and only widen with U+FE0F, which is a
  // sequence-level concern not handled at the codepoint level yet.
  CHECK(cell_width(0x2764) == 1); // heavy black heart ❤
  CHECK(cell_width(0x2600) == 1); // sun ☀
}

TEST_CASE("Cell derives width from its codepoint") {
  Cell narrow = Cell::from_char(U'A');
  CHECK(narrow.ch == U'A');
  CHECK(narrow.width == 1);

  Cell wide = Cell::from_char(U'中');
  CHECK(wide.ch == U'中');
  CHECK(wide.width == 2);

  Cell ctrl = Cell::from_char(U'\x1b');
  CHECK(ctrl.width == 0);
}

TEST_CASE("Cell equality compares ch, width, style") {
  CHECK(Cell::from_char(U'A') == Cell::from_char(U'A'));
  CHECK_FALSE(Cell::from_char(U'A') == Cell::from_char(U'B'));
}

// Unit tests for TextInputView (editing state + rendering).

#include <doctest/doctest.h>

#include <string>

#include "glyph/core/event.h"
#include "glyph/core/geometry.h"
#include "glyph/view/components/text_input.h"
#include "glyph/view/frame.h"

using namespace glyph;
using namespace glyph::core;
using glyph::view::TextInputView;

namespace {
  KeyEvent ch_key(char32_t c) {
    KeyEvent k{};
    k.code = KeyCode::Char;
    k.ch   = c;
    return k;
  }
  KeyEvent code_key(KeyCode code) {
    KeyEvent k{};
    k.code = code;
    return k;
  }
  // Read a row of a frame as a string (skips spacer cells).
  std::u32string row(const view::Frame &f, coord_t y) {
    std::u32string s;
    const auto      v = f.view();
    for (coord_t x = 0; x < f.size().w; ++x) {
      const auto &c = v.at(x, y);
      if (c.width == 0)
        continue;
      s.push_back(c.ch);
    }
    return s;
  }
} // namespace

TEST_CASE("typing inserts characters at the caret") {
  TextInputView in;
  in.handle_key(ch_key(U'h'));
  in.handle_key(ch_key(U'i'));
  CHECK(in.text() == U"hi");
  CHECK(in.caret() == 2);
}

TEST_CASE("control characters are not inserted as text") {
  TextInputView in;
  CHECK_FALSE(in.handle_key(ch_key(U'\t'))); // < ' '
  CHECK(in.text().empty());
}

TEST_CASE("backspace deletes before the caret") {
  TextInputView in{U"abc"};
  in.set_caret(3);
  CHECK(in.handle_key(code_key(KeyCode::Backspace)));
  CHECK(in.text() == U"ab");
  CHECK(in.caret() == 2);
}

TEST_CASE("backspace at start is a no-op") {
  TextInputView in{U"abc"};
  in.set_caret(0);
  CHECK_FALSE(in.handle_key(code_key(KeyCode::Backspace)));
  CHECK(in.text() == U"abc");
}

TEST_CASE("delete removes the character at the caret") {
  TextInputView in{U"abc"};
  in.set_caret(1);
  CHECK(in.handle_key(code_key(KeyCode::Delete)));
  CHECK(in.text() == U"ac");
  CHECK(in.caret() == 1);
}

TEST_CASE("caret motion: left/right/home/end") {
  TextInputView in{U"abc"};
  in.set_caret(1);
  in.handle_key(code_key(KeyCode::Left));
  CHECK(in.caret() == 0);
  in.handle_key(code_key(KeyCode::Left)); // clamp at 0
  CHECK(in.caret() == 0);
  in.handle_key(code_key(KeyCode::Right));
  CHECK(in.caret() == 1);
  in.handle_key(code_key(KeyCode::End));
  CHECK(in.caret() == 3);
  in.handle_key(code_key(KeyCode::Right)); // clamp at end
  CHECK(in.caret() == 3);
  in.handle_key(code_key(KeyCode::Home));
  CHECK(in.caret() == 0);
}

TEST_CASE("insert in the middle of the buffer") {
  TextInputView in{U"ac"};
  in.set_caret(1);
  in.handle_key(ch_key(U'b'));
  CHECK(in.text() == U"abc");
  CHECK(in.caret() == 2);
}

TEST_CASE("max_length rejects further insertion") {
  TextInputView in;
  in.set_max_length(3);
  for (char32_t c : std::u32string(U"abcd")) {
    in.handle_key(ch_key(c));
  }
  CHECK(in.text() == U"abc");
  CHECK(in.caret() == 3);
}

TEST_CASE("clear resets text and caret") {
  TextInputView in{U"hello"};
  in.clear();
  CHECK(in.empty());
  CHECK(in.caret() == 0);
}

TEST_CASE("unhandled keys return false") {
  TextInputView in;
  CHECK_FALSE(in.handle_key(code_key(KeyCode::Enter)));
  CHECK_FALSE(in.handle_key(code_key(KeyCode::Esc)));
}

TEST_CASE("render draws text into the frame") {
  view::Frame frame{Size{10, 1}};
  frame.fill(Cell::from_char(U' '));

  TextInputView in{U"hi"};
  in.set_show_cursor(false);
  in.render(frame, frame.bounds());

  CHECK(frame.view().at(0, 0).ch == U'h');
  CHECK(frame.view().at(1, 0).ch == U'i');
}

TEST_CASE("render places the caret at the caret column") {
  view::Frame frame{Size{10, 1}};
  frame.fill(Cell::from_char(U' '));

  TextInputView in{U"hi"};
  in.set_focused(true);
  in.set_caret(1); // on the 'i'
  in.render(frame, frame.bounds());

  // The caret cell still shows the underlying glyph 'i'.
  CHECK(frame.view().at(1, 0).ch == U'i');
}

TEST_CASE("wide glyphs advance the caret column by two") {
  view::Frame frame{Size{10, 1}};
  frame.fill(Cell::from_char(U' '));

  TextInputView in{U"中x"}; // 中 is width 2
  in.set_show_cursor(false);
  in.render(frame, frame.bounds());

  CHECK(frame.view().at(0, 0).ch == U'中');
  CHECK(frame.view().at(0, 0).width == 2);
  // 'x' lands at column 2 (after the wide glyph + its spacer).
  CHECK(frame.view().at(2, 0).ch == U'x');
}

TEST_CASE("horizontal scroll keeps the caret visible") {
  view::Frame frame{Size{4, 1}}; // narrow: 4 columns
  frame.fill(Cell::from_char(U'.'));

  TextInputView in{U"abcdefgh"};
  in.set_show_cursor(false);
  in.set_caret(8); // caret at end -> view should scroll right
  in.render(frame, frame.bounds());

  // Last 4 glyphs (e f g h) should be visible; 'a' must be scrolled off.
  const auto r = row(frame, 0);
  CHECK(r.find(U'h') != std::u32string::npos);
  CHECK(r.find(U'a') == std::u32string::npos);
}

TEST_CASE("placeholder shows when empty and hides once typed") {
  view::Frame frame{Size{12, 1}};
  frame.fill(Cell::from_char(U' '));

  TextInputView in;
  in.set_placeholder(U"type...");
  in.set_show_cursor(false);
  in.render(frame, frame.bounds());
  CHECK(row(frame, 0).find(U"type...") != std::u32string::npos);

  // After typing, placeholder is gone.
  view::Frame frame2{Size{12, 1}};
  frame2.fill(Cell::from_char(U' '));
  in.handle_key(ch_key(U'x'));
  in.render(frame2, frame2.bounds());
  CHECK(row(frame2, 0).find(U"type...") == std::u32string::npos);
  CHECK(frame2.view().at(0, 0).ch == U'x');
}

TEST_CASE("reports the hardware cursor at the caret column (IME anchor)") {
  view::Frame frame{Size{10, 1}};
  frame.fill(Cell::from_char(U' '));

  TextInputView in{U"abc"};
  in.set_focused(true);
  in.set_caret(2);
  in.render(frame, frame.bounds());

  const auto cur = frame.cursor();
  CHECK(cur.visible);
  CHECK(cur.pos == Point{2, 0});
}

TEST_CASE("cursor reported past a wide glyph accounts for its width") {
  view::Frame frame{Size{10, 1}};
  frame.fill(Cell::from_char(U' '));

  TextInputView in{U"中b"}; // 中 occupies columns 0-1, b at column 2
  in.set_caret(1);          // caret before 'b'
  in.render(frame, frame.bounds());

  CHECK(frame.cursor().visible);
  CHECK(frame.cursor().pos == Point{2, 0});
}

TEST_CASE("unfocused field does not report a cursor") {
  view::Frame frame{Size{10, 1}};
  frame.fill(Cell::from_char(U' '));

  TextInputView in{U"abc"};
  in.set_focused(false);
  in.render(frame, frame.bounds());
  CHECK_FALSE(frame.cursor().visible);
}

TEST_CASE("reverse-block caret keeps wide-glyph width (no spacing corruption)") {
  // With cursor reporting off, the caret paints a block; landing on a wide
  // glyph it must keep width 2 so the buffer's spacer pairing is intact.
  view::Frame frame{Size{10, 1}};
  frame.fill(Cell::from_char(U' '));

  TextInputView in{U"中b"};
  in.set_report_cursor(false);
  in.set_caret(0); // caret on the wide glyph
  in.render(frame, frame.bounds());

  CHECK(frame.view().at(0, 0).width == 2);
  CHECK(frame.view().at(1, 0).width == 0); // spacer intact
  CHECK(frame.view().at(2, 0).ch == U'b'); // 'b' still aligned at column 2
}

// Unit tests for the shared VT escape-sequence decoder.

#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "glyph/core/event.h"
#include "glyph/input/detail/vt_decoder.h"

using namespace glyph;
using namespace glyph::core;
using glyph::input::detail::VtDecoder;

namespace {
  // Feed a UTF-32 string, flush, and collect all resulting events.
  std::vector<Event> decode(const std::u32string &in) {
    VtDecoder dec;
    for (char32_t c : in) {
      dec.feed(c);
    }
    dec.flush(true);
    std::vector<Event> out;
    while (dec.has_event()) {
      out.push_back(dec.pop());
    }
    return out;
  }

  const KeyEvent &as_key(const Event &e) {
    REQUIRE(std::holds_alternative<KeyEvent>(e));
    return std::get<KeyEvent>(e);
  }
  const MouseEvent &as_mouse(const Event &e) {
    REQUIRE(std::holds_alternative<MouseEvent>(e));
    return std::get<MouseEvent>(e);
  }
} // namespace

TEST_CASE("printable characters become Char key events") {
  auto ev = decode(U"ab");
  REQUIRE(ev.size() == 2);
  CHECK(as_key(ev[0]).code == KeyCode::Char);
  CHECK(as_key(ev[0]).ch == U'a');
  CHECK(as_key(ev[1]).ch == U'b');
}

TEST_CASE("C0 control bytes map to named keys (POSIX path)") {
  CHECK(as_key(decode(U"\r")[0]).code == KeyCode::Enter);
  CHECK(as_key(decode(U"\n")[0]).code == KeyCode::Enter);
  CHECK(as_key(decode(U"\t")[0]).code == KeyCode::Tab);
  CHECK(as_key(decode(U"\b")[0]).code == KeyCode::Backspace);
  CHECK(as_key(decode(std::u32string(1, char32_t(0x7F)))[0]).code ==
        KeyCode::Backspace);
}

TEST_CASE("regression: text followed by CR sends Enter") {
  // This is the agent_chat bug: 'hi' + Enter must yield Char,Char,Enter.
  auto ev = decode(U"hi\r");
  REQUIRE(ev.size() == 3);
  CHECK(as_key(ev[0]).ch == U'h');
  CHECK(as_key(ev[1]).ch == U'i');
  CHECK(as_key(ev[2]).code == KeyCode::Enter);
}

TEST_CASE("CSI cursor keys") {
  CHECK(as_key(decode(U"\x1b[A")[0]).code == KeyCode::Up);
  CHECK(as_key(decode(U"\x1b[B")[0]).code == KeyCode::Down);
  CHECK(as_key(decode(U"\x1b[C")[0]).code == KeyCode::Right);
  CHECK(as_key(decode(U"\x1b[D")[0]).code == KeyCode::Left);
  CHECK(as_key(decode(U"\x1b[H")[0]).code == KeyCode::Home);
  CHECK(as_key(decode(U"\x1b[F")[0]).code == KeyCode::End);
}

TEST_CASE("SS3 cursor and function keys") {
  CHECK(as_key(decode(U"\x1bOA")[0]).code == KeyCode::Up);
  CHECK(as_key(decode(U"\x1bOP")[0]).code == KeyCode::F1);
  CHECK(as_key(decode(U"\x1bOS")[0]).code == KeyCode::F4);
}

TEST_CASE("CSI tilde navigation and function keys") {
  CHECK(as_key(decode(U"\x1b[2~")[0]).code == KeyCode::Insert);
  CHECK(as_key(decode(U"\x1b[3~")[0]).code == KeyCode::Delete);
  CHECK(as_key(decode(U"\x1b[5~")[0]).code == KeyCode::PageUp);
  CHECK(as_key(decode(U"\x1b[6~")[0]).code == KeyCode::PageDown);
  CHECK(as_key(decode(U"\x1b[15~")[0]).code == KeyCode::F5);
  CHECK(as_key(decode(U"\x1b[24~")[0]).code == KeyCode::F12);
}

TEST_CASE("lone ESC resolves to Esc on flush") {
  auto ev = decode(U"\x1b");
  REQUIRE(ev.size() == 1);
  CHECK(as_key(ev[0]).code == KeyCode::Esc);
}

TEST_CASE("SGR mouse press and release") {
  // CSI < 0 ; 5 ; 3 M  -> left button down at (col 5, row 3) -> 0-based (4,2)
  auto down = decode(U"\x1b[<0;5;3M");
  REQUIRE(down.size() == 1);
  const auto &m = as_mouse(down[0]);
  CHECK(m.button == MouseButton::Left);
  CHECK(m.action == MouseAction::Down);
  CHECK(m.pos == Point{4, 2});

  auto up = decode(U"\x1b[<0;5;3m");
  CHECK(as_mouse(up[0]).action == MouseAction::Up);
}

TEST_CASE("SGR mouse wheel") {
  // button code 64 = wheel up, 65 = wheel down
  CHECK(as_mouse(decode(U"\x1b[<64;1;1M")[0]).button == MouseButton::WheelUp);
  CHECK(as_mouse(decode(U"\x1b[<65;1;1M")[0]).button == MouseButton::WheelDown);
  CHECK(as_mouse(decode(U"\x1b[<64;1;1M")[0]).action == MouseAction::Scroll);
}

TEST_CASE("SGR mouse modifiers") {
  // base button 0 + shift(4) + ctrl(16) = 20
  auto        ev = decode(U"\x1b[<20;1;1M");
  REQUIRE(ev.size() == 1);
  const auto &m = as_mouse(ev[0]);
  CHECK(has_mod(m.mods, Mod::Shift));
  CHECK(has_mod(m.mods, Mod::Ctrl));
  CHECK_FALSE(has_mod(m.mods, Mod::Alt));
}

TEST_CASE("bracketed paste produces a PasteEvent") {
  auto ev = decode(U"\x1b[200~hello\x1b[201~");
  REQUIRE(ev.size() == 1);
  REQUIRE(std::holds_alternative<PasteEvent>(ev[0]));
  CHECK(std::get<PasteEvent>(ev[0]).text == U"hello");
}

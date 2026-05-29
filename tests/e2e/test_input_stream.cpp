// End-to-end input tests: raw terminal byte stream -> core::Event sequence.
//
// PosixInput's termios/poll layer needs a real TTY, so the testable E2E
// boundary is "bytes a terminal would send" -> UTF-8 decode -> VtDecoder
// -> events. We mirror the backend's incremental UTF-8 decode here and
// drive the same shared decoder the backend uses.

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "glyph/core/event.h"
#include "glyph/input/detail/vt_decoder.h"

using namespace glyph;
using namespace glyph::core;
using glyph::input::detail::VtDecoder;

namespace {
  // Incremental UTF-8 decode matching PosixInput::feed_bytes semantics.
  std::vector<Event> decode_bytes(const std::string &bytes) {
    VtDecoder    dec;
    char32_t     cp      = 0;
    std::uint8_t need    = 0;

    for (unsigned char byte : bytes) {
      if (need == 0) {
        if (byte < 0x80) {
          dec.feed(static_cast<char32_t>(byte));
        } else if ((byte & 0xE0) == 0xC0) {
          cp = byte & 0x1F; need = 1;
        } else if ((byte & 0xF0) == 0xE0) {
          cp = byte & 0x0F; need = 2;
        } else if ((byte & 0xF8) == 0xF0) {
          cp = byte & 0x07; need = 3;
        }
      } else if ((byte & 0xC0) == 0x80) {
        cp = (cp << 6) | (byte & 0x3F);
        if (--need == 0) {
          dec.feed(cp);
          cp = 0;
        }
      }
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
} // namespace

TEST_CASE("E2E: ASCII text then Enter") {
  auto ev = decode_bytes("hi\r");
  REQUIRE(ev.size() == 3);
  CHECK(as_key(ev[0]).ch == U'h');
  CHECK(as_key(ev[1]).ch == U'i');
  CHECK(as_key(ev[2]).code == KeyCode::Enter);
}

TEST_CASE("E2E: multi-byte UTF-8 decodes to a single codepoint") {
  // U+4E2D '中' is E4 B8 AD in UTF-8.
  auto ev = decode_bytes("\xE4\xB8\xAD");
  REQUIRE(ev.size() == 1);
  CHECK(as_key(ev[0]).code == KeyCode::Char);
  CHECK(as_key(ev[0]).ch == U'中');
}

TEST_CASE("E2E: 4-byte UTF-8 (emoji) decodes correctly") {
  // U+1F600 grinning face is F0 9F 98 80.
  auto ev = decode_bytes("\xF0\x9F\x98\x80");
  REQUIRE(ev.size() == 1);
  CHECK(as_key(ev[0]).ch == U'\U0001F600');
}

TEST_CASE("E2E: mixed ASCII and multi-byte stream") {
  auto ev = decode_bytes("a\xE4\xB8\xAD" "b");
  REQUIRE(ev.size() == 3);
  CHECK(as_key(ev[0]).ch == U'a');
  CHECK(as_key(ev[1]).ch == U'中');
  CHECK(as_key(ev[2]).ch == U'b');
}

TEST_CASE("E2E: arrow key escape sequence from the wire") {
  // What a terminal sends for the Up arrow.
  auto ev = decode_bytes("\x1b[A");
  REQUIRE(ev.size() == 1);
  CHECK(as_key(ev[0]).code == KeyCode::Up);
}

TEST_CASE("E2E: SGR mouse click from the wire") {
  auto ev = decode_bytes("\x1b[<0;10;5M");
  REQUIRE(ev.size() == 1);
  REQUIRE(std::holds_alternative<MouseEvent>(ev[0]));
  const auto &m = std::get<MouseEvent>(ev[0]);
  CHECK(m.action == MouseAction::Down);
  CHECK(m.pos == Point{9, 4}); // 1-based 10,5 -> 0-based 9,4
}

TEST_CASE("E2E: bracketed paste with UTF-8 payload") {
  auto ev = decode_bytes("\x1b[200~a\xE4\xB8\xAD\x1b[201~");
  REQUIRE(ev.size() == 1);
  REQUIRE(std::holds_alternative<PasteEvent>(ev[0]));
  CHECK(std::get<PasteEvent>(ev[0]).text == U"a中");
}

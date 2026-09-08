// Unit tests for the FullScreenGuard RAII terminal-state management.

#include <doctest/doctest.h>

#include <stdexcept>
#include <sstream>
#include <string>

#include "glyph/render/terminal.h"

using namespace glyph::render;

TEST_CASE("FullScreenGuard emits enter sequences on construction") {
  std::ostringstream oss;
  {
    FullScreenGuard guard{oss};
    CHECK(oss.str() == "\x1b[?1049h\x1b[?25l\x1b[?7l");
  }
  CHECK(oss.str() ==
        "\x1b[?1049h\x1b[?25l\x1b[?7l"
        "\x1b[?25h\x1b[?7h\x1b[?1049l");
}

TEST_CASE("FullScreenGuard restores state on exception unwind") {
  std::ostringstream oss;
  bool               threw = false;
  try {
    FullScreenGuard guard{oss};
    throw std::runtime_error("boom");
  } catch (const std::runtime_error &) {
    threw = true;
  }
  CHECK(threw);
  CHECK(oss.str().find("\x1b[?1049l") != std::string::npos);
  CHECK(oss.str().find("\x1b[?7h") != std::string::npos);
}

TEST_CASE("FullScreenGuard can be re-entered after destruction") {
  std::ostringstream a, b;
  {
    FullScreenGuard g{a};
  }
  {
    FullScreenGuard g{b};
  }
  CHECK(a.str() == b.str());
}

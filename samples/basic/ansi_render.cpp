// samples/basic/ansi_render.cpp
//
// Dirty-aware demo:
// - Frame persists across frames
// - Background drawn once
// - Only moving text line + progress line updated per frame
// - Exit on Esc or Q/q

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <thread>
#include <variant>

#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/render/ansi/ansi_renderer.h"
#include "glyph/view/frame.h"

#include "glyph/input/input_guard.h"
#include "glyph/input/win_input.h"

namespace {

  using namespace glyph;

  void draw_border(view::Frame &f, core::Rect r) {
    if (r.empty())
      return;

    const core::coord_t x0 = r.left();
    const core::coord_t y0 = r.top();
    const core::coord_t x1 = r.right() - 1;
    const core::coord_t y1 = r.bottom() - 1;

    const core::Cell h = core::Cell::from_char(U'-');
    const core::Cell v = core::Cell::from_char(U'|');
    const core::Cell c = core::Cell::from_char(U'+');

    for (core::coord_t x = x0 + 1; x < x1; ++x) {
      f.set(core::Point{x, y0}, h);
      f.set(core::Point{x, y1}, h);
    }

    for (core::coord_t y = y0 + 1; y < y1; ++y) {
      f.set(core::Point{x0, y}, v);
      f.set(core::Point{x1, y}, v);
    }

    f.set(core::Point{x0, y0}, c);
    f.set(core::Point{x1, y0}, c);
    f.set(core::Point{x0, y1}, c);
    f.set(core::Point{x1, y1}, c);
  }

  void draw_text(view::Frame &f, core::Point p, const char32_t *text) {
    for (core::coord_t i = 0; text[i] != U'\0'; ++i) {
      f.set(core::Point{p.x + i, p.y}, core::Cell::from_char(text[i]));
    }
  }

  void clear_text(view::Frame &f, core::Point p, core::coord_t len) {
    for (core::coord_t i = 0; i < len; ++i) {
      f.set(core::Point{p.x + i, p.y}, core::Cell::from_char(U' '));
    }
  }

  void draw_progress(view::Frame &f, core::Rect r, float t01) {
    if (r.empty())
      return;

    const core::coord_t total = r.width();
    const core::coord_t filled =
        static_cast<core::coord_t>(std::round(t01 * total));

    for (core::coord_t i = 0; i < total; ++i) {
      const char32_t ch = (i < filled) ? U'#' : U'.';
      f.set(core::Point{r.left() + i, r.top()}, core::Cell::from_char(ch));
    }
  }

  struct CursorGuard {
    explicit CursorGuard(std::ostream &out) : out_(out) {
      out_ << "\x1b[?25l";
    }
    ~CursorGuard() {
      out_ << "\x1b[?25h";
    }
    std::ostream &out_;
  };

  bool handle_input(glyph::input::WinInput &input) {
    auto ev = input.poll();
    if (std::holds_alternative<glyph::core::KeyEvent>(ev)) {
      const auto &k = std::get<glyph::core::KeyEvent>(ev);
      if (k.code == glyph::core::KeyCode::Esc)
        return true;
      if (k.code == glyph::core::KeyCode::Char) {
        if (k.ch == U'q' || k.ch == U'Q')
          return true;
      }
    }
    return false;
  }

} // namespace

int main() {
  using namespace glyph;

  render::AnsiRenderer renderer{std::cout};
  CursorGuard          cursor{std::cout};

  input::WinInput   input;
  input::InputGuard input_guard(input, input::InputMode::Raw);

  const core::Size size{48, 14};
  const char32_t   title[] = U" Glyph Dirty Demo ";

  // Persistent frame: draw background once.
  view::Frame framebuf{size};
  framebuf.fill(core::Cell::from_char(U'.'));
  draw_border(framebuf, framebuf.bounds());
  draw_text(framebuf, core::Point{2, 0}, title);

  core::coord_t x   = 2;
  core::coord_t dir = 1;

  for (std::uint64_t frame = 0;; ++frame) {
    if (handle_input(input)) {
      break;
    }

    // Update only the moving text line.
    const core::coord_t y = 5;
    clear_text(framebuf, core::Point{2, y}, size.w - 4);
    const char32_t moving[] = U"Moving";
    draw_text(framebuf, core::Point{x, y}, moving);

    // Update progress line only.
    const float t01 = static_cast<float>((frame % 100) / 100.0);
    draw_progress(framebuf, core::Rect{2, 10, size.w - 4, 1}, t01);

    renderer.render(framebuf);

    x += dir;
    if (x <= 2 || x + 6 >= size.w - 2) {
      dir = -dir;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(60));
  }

  return 0;
}
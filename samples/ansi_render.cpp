// samples/ansi_render.cpp
//
// Rich ANSI renderer demo:
// - Full-frame redraw
// - Border + title
// - Bouncing text
// - Progress bar
// - Basic FPS estimate
//
// Exit: Ctrl+C (no input handling in this minimal sample).

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <thread>

#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/render/ansi/ansi_renderer.h"
#include "glyph/view/frame.h"

namespace {

  using namespace glyph;

  // Draw a simple single-line border around a rect.
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

    // Horizontal lines
    for (core::coord_t x = x0 + 1; x < x1; ++x) {
      f.set(core::Point{x, y0}, h);
      f.set(core::Point{x, y1}, h);
    }

    // Vertical lines
    for (core::coord_t y = y0 + 1; y < y1; ++y) {
      f.set(core::Point{x0, y}, v);
      f.set(core::Point{x1, y}, v);
    }

    // Corners
    f.set(core::Point{x0, y0}, c);
    f.set(core::Point{x1, y0}, c);
    f.set(core::Point{x0, y1}, c);
    f.set(core::Point{x1, y1}, c);
  }

  // Draw ASCII text at a position (no clipping beyond Frame::set).
  void draw_text(view::Frame &f, core::Point p, const char32_t *text) {
    for (core::coord_t i = 0; text[i] != U'\0'; ++i) {
      f.set(core::Point{p.x + i, p.y}, core::Cell::from_char(text[i]));
    }
  }

  // Draw a progress bar in a given rect.
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

  // RAII helper to hide cursor during animation.
  struct CursorGuard {
    explicit CursorGuard(std::ostream &out) : out_(out) {
      out_ << "\x1b[?25l"; // hide cursor
    }
    ~CursorGuard() {
      out_ << "\x1b[?25h"; // show cursor
    }
    std::ostream &out_;
  };

} // namespace

int main() {
  using namespace glyph;

  // Renderer writes ANSI sequences to stdout.
  render::AnsiRenderer renderer{std::cout};
  CursorGuard          cursor{std::cout};

  const core::Size size{48, 14};
  const char32_t   title[] = U" Glyph ANSI Demo ";

  // Animation state
  core::coord_t x   = 2;
  core::coord_t dir = 1;

  // Timing for FPS estimate
  auto  last = std::chrono::steady_clock::now();
  float fps  = 0.0f;

  for (std::uint64_t frame = 0;; ++frame) {
    view::Frame framebuf{size};

    // 1) Clear with background dots
    framebuf.fill(core::Cell::from_char(U'.'));

    // 2) Draw border and title
    const core::Rect bounds = framebuf.bounds();
    draw_border(framebuf, bounds);
    draw_text(framebuf, core::Point{2, 0}, title);

    // 3) Moving text
    const char32_t      moving[] = U"Moving";
    const core::coord_t y        = 5;
    draw_text(framebuf, core::Point{x, y}, moving);

    // 4) Progress bar based on time
    const float t01 = static_cast<float>((frame % 100) / 100.0);
    draw_progress(framebuf, core::Rect{2, 10, size.w - 4, 1}, t01);

    // 5) FPS display (simple moving average)
    auto        now = std::chrono::steady_clock::now();
    const float dt =
        std::chrono::duration_cast<std::chrono::duration<float>>(now - last)
            .count();
    last                = now;
    const float instant = (dt > 0.0f) ? (1.0f / dt) : fps;
    fps                 = fps * 0.9f + instant * 0.1f;

    // Format FPS into a small text buffer.
    char fps_buf[32];
    std::snprintf(fps_buf, sizeof(fps_buf), "FPS: %4.1f", fps);
    // Draw ASCII fps buffer as UTF-32 (simple widen)
    char32_t fps_u32[32]{};
    for (std::size_t i = 0; fps_buf[i] != '\0'; ++i) {
      fps_u32[i] = static_cast<char32_t>(fps_buf[i]);
    }
    draw_text(framebuf, core::Point{2, 12}, fps_u32);

    // 6) Render full frame
    renderer.render(framebuf);

    // 7) Update animation
    x += dir;
    if (x <= 2 || x + 6 >= size.w - 2) {
      dir = -dir;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(60));
  }

  return 0;
}
#include "glyph/core/buffer.h"

using namespace glyph::core;

void draw_box(BufferView v) {
  const Cell border{U'#', Style{.fg = 7, .bg = 0, .attrs = 0}};
  const Rect b = v.bounds();
  if (b.empty())
    return;

  for (coord_t x = 0; x < b.width(); ++x) {
    v.at(x, 0)              = border;
    v.at(x, b.height() - 1) = border;
  }
  for (coord_t y = 0; y < b.height(); ++y) {
    v.at(0, y)             = border;
    v.at(b.width() - 1, y) = border;
  }
}

int main() {
  Buffer buf(Size{80, 24}, Cell{U' ', Style{}});
  buf.fill(Cell{U'.', Style{.fg = 8}});

  auto panel = buf.subview(Rect{2, 2, 30, 10});
  panel.fill(Cell{U' ', Style{.bg = 4}});
  draw_box(panel);

  // ...
}

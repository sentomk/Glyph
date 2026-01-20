#include <iostream>

import glyph;

int main() {
  using namespace glyph;
  render::TerminalSessionOptions options{};
  options.use_alt_screen = false;
  options.hide_cursor    = false;
  render::TerminalApp app{std::cout, options};
  view::Frame         frame{core::Size{40, 10}};
  auto                label =
      view::LabelView(U"Hello Modules")
          .set_align(view::layout::AlignH::Center, view::layout::AlignV::Center)
          .set_cell(
              core::Cell::from_char(U' ', core::Style{}.fg(0xECEFF4).bold()));
  label.render(frame, frame.bounds());
  app.render(frame);
  std::cout.flush();
  std::cin.get();
  return 0;
}

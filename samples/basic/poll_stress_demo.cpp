// samples/basic/poll_stress_demo.cpp
//
// Poll stress demo: exercise input polling and rendering at a fixed cadence.

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "glyph/core/cell.h"
#include "glyph/core/style.h"
#include "glyph/input/input_guard.h"
#include "glyph/input/win32/win_input.h"
#include "glyph/render/terminal.h"
#include "glyph/view/components/label.h"
#include "glyph/view/components/panel.h"
#include "glyph/view/components/stack.h"
#include "glyph/view/frame.h"

namespace {

  using namespace glyph;

  std::u32string to_u32(std::uint64_t value) {
    const auto   s = std::to_string(value);
    std::u32string out;
    out.reserve(s.size());
    for (char ch : s) {
      out.push_back(static_cast<char32_t>(ch));
    }
    return out;
  }

  void render_frame(view::Frame &frame, std::uint64_t polls,
                    std::uint64_t events, std::uint64_t frames,
                    std::uint64_t fps) {
    auto fg = core::Style{}.fg(0xECEFF4);
    auto dim = core::Style{}.fg(0xA3BE8C).dim();

    auto title = view::LabelView(U"Glyph Poll Stress")
                     .set_align(view::layout::AlignH::Center,
                                view::layout::AlignV::Center)
                     .set_cell(core::Cell::from_char(U' ', fg.bold()));
    auto header = view::PanelView::header(&title, 0x88C0D0);

    std::u32string line1 = U"poll calls: ";
    line1 += to_u32(polls);
    std::u32string line2 = U"events read: ";
    line2 += to_u32(events);
    std::u32string line3 = U"frames     : ";
    line3 += to_u32(frames);
    std::u32string line4 = U"fps (avg)  : ";
    line4 += to_u32(fps);

    auto stats =
        view::LabelView(line1 + U"\n" + line2 + U"\n" + line3 + U"\n" + line4)
            .set_align(view::layout::AlignH::Left,
                       view::layout::AlignV::Top)
            .set_cell(core::Cell::from_char(U' ', dim));

    auto stats_panel = view::PanelView::card(&stats, 0xA3BE8C);

    auto footer = view::LabelView(U"Press Q to exit")
                      .set_align(view::layout::AlignH::Center,
                                 view::layout::AlignV::Center)
                      .set_cell(core::Cell::from_char(U' ', fg));

    auto layout = view::VStack(
        {
            view::Fixed(header, 3),
            view::Flex(stats_panel, 1),
            view::Fixed(footer, 2),
        },
        1);

    layout.render(frame, frame.bounds());
  }

} // namespace

int main() {
  using namespace glyph;
  using namespace std::chrono_literals;

  render::TerminalApp app{std::cout};
  input::WinInput     input{};
  input::InputGuard   guard(input, input::InputMode::Raw);

  std::uint64_t polls = 0;
  std::uint64_t events = 0;
  std::uint64_t frames = 0;

  const auto start = std::chrono::steady_clock::now();
  auto       next_frame = start;
  const auto frame_step = 16ms;

  for (;;) {
    ++polls;
    if (auto ev = input.poll();
        std::holds_alternative<core::KeyEvent>(ev)) {
      ++events;
      const auto &key = std::get<core::KeyEvent>(ev);
      if (key.code == core::KeyCode::Char &&
          (key.ch == U'q' || key.ch == U'Q')) {
        break;
      }
    }

    const auto now = std::chrono::steady_clock::now();
    if (now < next_frame) {
      std::this_thread::sleep_until(next_frame);
    }
    next_frame += frame_step;

    const auto size = app.frame_size({80, 18});
    if (size.w <= 0 || size.h <= 0) {
      continue;
    }

    view::Frame frame{size, core::Cell::from_char(U' ')};
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(now - start);
    const auto seconds = std::max<std::uint64_t>(1, elapsed.count());
    const auto fps = frames / seconds;
    render_frame(frame, polls, events, frames, fps);
    app.render(frame);
    ++frames;
  }

  return 0;
}

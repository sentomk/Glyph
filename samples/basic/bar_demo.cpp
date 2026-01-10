// samples/basic/bar_demo.cpp
//
// Bar demo: shows BarView with a title bar and a progress bar.

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/core/style.h"
#include "glyph/render/terminal.h"
#include "glyph/view/components/bar.h"
#include "glyph/view/components/label.h"
#include "glyph/view/components/stack.h"
#include "glyph/view/frame.h"

namespace {

  using namespace glyph;

  class ProgressView final : public view::View {
  public:
    explicit ProgressView(core::Cell fill) : fill_(fill) {
    }

    void set_value(int value) {
      value_ = std::clamp(value, 0, 100);
    }

    void render(view::Frame &f, core::Rect area) const override {
      if (area.empty() || value_ <= 0) {
        return;
      }
      const auto filled = core::coord_t((area.size.w * value_) / 100);
      if (filled <= 0) {
        return;
      }
      const core::Rect bar{area.origin, core::Size{filled, area.size.h}};
      f.fill_rect(bar, fill_);
    }

  private:
    int       value_ = 0;
    core::Cell fill_{};
  };

  std::u32string to_u32(int value) {
    const auto   s = std::to_string(value);
    std::u32string out;
    out.reserve(s.size());
    for (char ch : s) {
      out.push_back(static_cast<char32_t>(ch));
    }
    return out;
  }

} // namespace

int main() {
  using namespace glyph;
  using namespace std::chrono_literals;

  render::TerminalApp app{std::cout};
  int                 progress = 0;

  for (;;) {
    const auto size = app.frame_size({80, 12});
    if (size.w <= 0 || size.h <= 0) {
      std::this_thread::sleep_for(50ms);
      continue;
    }

    view::Frame frame{size, core::Cell::from_char(U' ')};

    const auto title_style = core::Style{}.fg(0xECEFF4).bold();
    const auto title_cell  = core::Cell::from_char(U' ', title_style);
    auto       title_left  = view::LabelView(U"Glyph Bar Demo")
                            .set_align(view::layout::AlignH::Left,
                                       view::layout::AlignV::Center)
                            .set_cell(title_cell);
    auto       title_right = view::LabelView(U"F1 Help  |  Q Quit")
                             .set_align(view::layout::AlignH::Right,
                                        view::layout::AlignV::Center)
                             .set_cell(title_cell);

    const auto bar_bg = core::Cell::from_char(
        U' ', core::Style{}.bg(0x2E3440));
    auto top_overlay = view::ZStackView({&title_left, &title_right});
    view::BarView top_bar(&top_overlay, bar_bg);

    const auto progress_fill = core::Cell::from_char(
        U' ', core::Style{}.bg(0x5E81AC));
    ProgressView progress_view(progress_fill);
    progress_view.set_value(progress);

    const auto percent = static_cast<int>(progress);
    std::u32string text = U"Loading ";
    text += to_u32(percent);
    text += U"%";

    auto progress_label =
        view::LabelView(text)
            .set_align(view::layout::AlignH::Center,
                       view::layout::AlignV::Center)
            .set_cell(core::Cell::from_char(
                U' ', core::Style{}.fg(0xECEFF4)));

    auto overlay = view::ZStackView({&progress_view, &progress_label});
    view::BarView bottom_bar(&overlay, bar_bg);

    const auto full = frame.bounds();
    const auto top =
        core::Rect{full.origin, core::Size{full.size.w, 1}};
    const auto bottom = core::Rect{
        core::Point{0, core::coord_t(full.size.h - 1)},
        core::Size{full.size.w, 1}};
    const auto body = core::Rect{
        core::Point{0, 1},
        core::Size{full.size.w, core::coord_t(full.size.h - 2)}};

    top_bar.render(frame, top);
    bottom_bar.render(frame, bottom);

    auto hint =
        view::LabelView(U"Press Ctrl+C to exit")
            .set_align(view::layout::AlignH::Center,
                       view::layout::AlignV::Center)
            .set_cell(core::Cell::from_char(
                U' ', core::Style{}.fg(0x4C566A)));
    hint.render(frame, body);

    app.render(frame);

    progress = (progress + 1) % 101;
    std::this_thread::sleep_for(50ms);
  }

  return 0;
}

// samples/ui/aurora_dashboard.cpp
//
// Minimal UI showcase: layered background, panels, and labels.

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

#include "glyph/core/style.h"
#include "glyph/input/input_guard.h"
#include "glyph/input/win32/win_input.h"
#include "glyph/render/terminal.h"
#include "glyph/view/components/label.h"
#include "glyph/view/components/panel.h"
#include "glyph/view/components/stack.h"
#include "glyph/view/layout/inset.h"
#include "glyph/view/view.h"

namespace {

  using namespace glyph;

  core::Cell label_cell(core::Color color, bool bold = false) {
    auto style = core::Style{}.fg(color);
    if (bold) {
      style = style.bold();
    }
    return core::Cell(U' ', style);
  }

  class BackgroundView final : public view::View {
  public:
    explicit BackgroundView(core::coord_t phase) : phase_(phase) {
    }

    void render(view::Frame &f, core::Rect area) const override {
      if (area.empty()) {
        return;
      }

      const core::Style sky    = core::Style{}.fg(0x3B4252).dim();
      const core::Style dusk   = core::Style{}.fg(0x4C566A).dim();
      const char32_t    dots[] = U" .:-=+*";

      for (core::coord_t y = area.top(); y < area.bottom(); ++y) {
        for (core::coord_t x = area.left(); x < area.right(); ++x) {
          const auto t = (x + y + phase_) % 7;
          core::Cell c(dots[t], (t < 3) ? sky : dusk);
          f.set({x, y}, c);
        }
      }
    }

  private:
    core::coord_t phase_ = 0;
  };

  class ToastView final : public view::View {
  public:
    explicit ToastView(std::u32string text) : text_(std::move(text)) {
    }

    void render(view::Frame &f, core::Rect area) const override {
      if (area.empty()) {
        return;
      }

      auto label = view::LabelView(text_)
                       .set_align(view::layout::AlignH::Center,
                                  view::layout::AlignV::Center)
                       .set_cell(label_cell(0x88C0D0, true));
      auto panel = view::PanelView::card(&label, 0x88C0D0, U'*',
                                         view::layout::Insets::hv(2, 1));

      const auto toast_w = std::min<core::coord_t>(28, area.width());
      const auto toast_h = core::coord_t(3);
      const auto origin  = core::Point{
          core::coord_t(area.right() - toast_w - 2),
          core::coord_t(area.top() + 1)};
      panel.render(f, core::Rect{origin, core::Size{toast_w, toast_h}});
    }

  private:
    std::u32string text_;
  };

  void render_ui(view::Frame &frame, core::coord_t phase, int focus) {
    using namespace glyph;

    const auto active_color = 0xE5E9F0;
    const auto nord_blue    = 0x88C0D0;

    BackgroundView bg(phase);
    const auto     card_base = view::PanelStyle::card(nord_blue);

    auto title = view::LabelView(U"Glyph - Aurora")
                     .set_align(view::layout::AlignH::Center,
                                view::layout::AlignV::Center)
                     .set_cell(label_cell(nord_blue, true));
    auto header =
        view::PanelView::card(&title, nord_blue, U'=',
                              view::layout::Insets::hv(2, 1));

    auto hero = view::LabelView(U"NOW PLAYING\n"
                                U"Neon Drift - 3:42\n"
                                U"Ambient / 124 bpm\n\n"
                                U"Queue: 12 tracks")
                    .set_align(view::layout::AlignH::Left,
                               view::layout::AlignV::Top)
                    .set_cell(label_cell(active_color));
    auto hero_panel =
        view::PanelView::card(&hero, focus == 0 ? active_color : nord_blue,
                              U'#');

    auto stats = view::LabelView(U"ACTIVE\n"
                                 U"- 24 nodes\n"
                                 U"- 3.2ms\n"
                                 U"- 99.99%")
                     .set_align(view::layout::AlignH::Left,
                                view::layout::AlignV::Top)
                     .set_cell(label_cell(0xA3BE8C));
    auto stats_panel = view::PanelView::card(
        &stats, focus == 1 ? active_color : core::Color{0xA3BE8C});

    auto alerts = view::LabelView(U"ALERTS\n"
                                  U"- None\n"
                                  U"- Systems nominal")
                      .set_align(view::layout::AlignH::Left,
                                 view::layout::AlignV::Top)
                      .set_cell(label_cell(0xD08770));
    auto alerts_panel = view::PanelView::card(
        &alerts, focus == 2 ? active_color : core::Color{0xD08770});

    auto right_stack = view::VStack(
        {
            view::Fixed(stats_panel, 6),
            view::Fixed(alerts_panel, 5),
        },
        1);

    auto body = view::HStack(
        {
            view::Flex(hero_panel, 2),
            view::Flex(right_stack, 1),
        },
        2);

    auto footer =
        view::LabelView(U"Press Q to exit - Tab to cycle focus - Built with Glyph")
            .set_align(view::layout::AlignH::Center,
                       view::layout::AlignV::Center)
            .set_cell(label_cell(active_color));

    auto layout = view::VStack(
        {
            view::Fixed(header, 4),
            view::Flex(body, 1),
            view::Fixed(footer, 2),
        },
        1);

    ToastView toast(U"Connected");

    auto root = view::ZStackView({&bg, &layout, &toast});
    root.render(frame, frame.bounds());
  }

} // namespace

int main() {
  using namespace glyph;
  using namespace std::chrono_literals;

  render::TerminalApp  app{std::cout};
  input::WinInput      input{};
  input::InputGuard    input_guard(input, input::InputMode::Raw);
  render::TerminalSize last{};
  core::Size           last_size{};
  bool                 first = true;
  core::coord_t        phase = 0;
  int                  focus = 0;

  for (;;) {
    const auto term = app.size();
    const auto size =
        core::Size{term.valid ? term.cols : 80, term.valid ? term.rows : 24};

    if (size.w <= 0 || size.h <= 0) {
      std::this_thread::sleep_for(50ms);
      continue;
    }

    if (term.valid && (last.cols != term.cols || last.rows != term.rows ||
                       last.valid != term.valid)) {
      last = term;
    }

    if (auto ev = input.poll(); std::holds_alternative<core::KeyEvent>(ev)) {
      const auto &key = std::get<core::KeyEvent>(ev);
      if (key.code == core::KeyCode::Esc) {
        break;
      }
      if (key.code == core::KeyCode::Char &&
          (key.ch == U'q' || key.ch == U'Q')) {
        break;
      }
      if (key.code == core::KeyCode::Tab ||
          (key.code == core::KeyCode::Char &&
           (key.ch == U'\t' || key.ch == U'T'))) {
        focus = (focus + 1) % 3;
        first = true;
      }
    }

    phase = core::coord_t((phase + 1) % 7);

    if (!first && size == last_size && (phase % 2 == 0)) {
      std::this_thread::sleep_for(80ms);
      continue;
    }

    view::Frame frame{size};
    render_ui(frame, phase, focus);
    app.render(frame);

    last_size = size;
    first     = false;
    std::this_thread::sleep_for(50ms);
  }

  return 0;
}

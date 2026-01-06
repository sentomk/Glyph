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

      view::LabelView label(text_);
      label.set_align(
          view::layout::AlignH::Center, view::layout::AlignV::Center);
      label.set_cell(core::Cell(U' ', core::Style{}.fg(0x88C0D0).bold()));

      view::PanelView panel(&label);
      panel.set_fill(core::Cell(U' '));
      panel.set_border(core::Cell(U'*', core::Style{}.fg(0x88C0D0)));
      panel.set_padding(view::layout::Insets::hv(2, 1));
      panel.set_draw_fill(true);
      panel.set_draw_border(true);

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

    BackgroundView bg(phase);

    view::LabelView title(U"Glyph - Aurora");
    title.set_align(view::layout::AlignH::Center, view::layout::AlignV::Center);
    title.set_cell(core::Cell(U' ', core::Style{}.fg(0x88C0D0).bold()));

    view::PanelView header(&title);
    header.set_fill(core::Cell(U' '));
    header.set_border(core::Cell(U'=', core::Style{}.fg(0x88C0D0)));
    header.set_padding(view::layout::Insets::hv(2, 1));
    header.set_draw_fill(true);
    header.set_draw_border(true);

    view::LabelView hero(
        U"NOW PLAYING\n"
        U"Neon Drift - 3:42\n"
        U"Ambient / 124 bpm\n\n"
        U"Queue: 12 tracks");
    hero.set_align(view::layout::AlignH::Left, view::layout::AlignV::Top);
    hero.set_cell(core::Cell(U' ', core::Style{}.fg(0xE5E9F0)));

    view::PanelView hero_panel(&hero);
    hero_panel.set_fill(core::Cell(U' '));
    hero_panel.set_border(
        core::Cell(U'#', core::Style{}.fg(focus == 0 ? 0xE5E9F0 : 0x88C0D0)));
    hero_panel.set_padding(view::layout::Insets::all(1));
    hero_panel.set_draw_fill(true);
    hero_panel.set_draw_border(true);

    view::LabelView stats(
        U"ACTIVE\n"
        U"- 24 nodes\n"
        U"- 3.2ms\n"
        U"- 99.99%");
    stats.set_align(view::layout::AlignH::Left, view::layout::AlignV::Top);
    stats.set_cell(core::Cell(U' ', core::Style{}.fg(0xA3BE8C)));

    view::PanelView stats_panel(&stats);
    stats_panel.set_fill(core::Cell(U' '));
    stats_panel.set_border(
        core::Cell(U'+', core::Style{}.fg(focus == 1 ? 0xE5E9F0 : 0xA3BE8C)));
    stats_panel.set_padding(view::layout::Insets::all(1));
    stats_panel.set_draw_fill(true);
    stats_panel.set_draw_border(true);

    view::LabelView alerts(
        U"ALERTS\n"
        U"- None\n"
        U"- Systems nominal");
    alerts.set_align(view::layout::AlignH::Left, view::layout::AlignV::Top);
    alerts.set_cell(core::Cell(U' ', core::Style{}.fg(0xD08770)));

    view::PanelView alerts_panel(&alerts);
    alerts_panel.set_fill(core::Cell(U' '));
    alerts_panel.set_border(
        core::Cell(U'+', core::Style{}.fg(focus == 2 ? 0xE5E9F0 : 0xD08770)));
    alerts_panel.set_padding(view::layout::Insets::all(1));
    alerts_panel.set_draw_fill(true);
    alerts_panel.set_draw_border(true);

    auto right_stack = view::VStack(
        {
            view::fixed(&stats_panel, 6),
            view::fixed(&alerts_panel, 5),
        },
        1);

    auto body = view::HStack(
        {
            view::flex(&hero_panel, 2),
            view::flex(&right_stack, 1),
        },
        2);

    view::LabelView footer(
        U"Press Q to exit - Tab to cycle focus - Built with Glyph");
    footer.set_align(
        view::layout::AlignH::Center, view::layout::AlignV::Center);
    footer.set_cell(core::Cell(U' ', core::Style{}.fg(0xE5E9F0)));

    auto layout = view::VStack(
        {
            view::fixed(&header, 4),
            view::flex(&body, 1),
            view::fixed(&footer, 2),
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

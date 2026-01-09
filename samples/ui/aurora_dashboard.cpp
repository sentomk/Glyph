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

  // Build a label cell with the desired color and emphasis.
  core::Cell label_cell(core::Color color, bool bold = false) {
    auto style = core::Style{}.fg(color);
    if (bold) {
      style = style.bold();
    }
    return core::Cell(U' ', style);
  }

  namespace ui {
    // Short-hand for a styled label with explicit alignment.
    inline view::LabelView label(std::u32string text, core::Color color,
                                 view::layout::AlignH align_h,
                                 view::layout::AlignV align_v,
                                 bool bold = false) {
      return view::LabelView(std::move(text))
          .set_align(align_h, align_v)
          .set_cell(label_cell(color, bold));
    }
  } // namespace ui

  // Animated starfield-like backdrop that shifts with phase.
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

  // Small overlay panel rendered in the top-right corner.
  class ToastView final : public view::View {
  public:
    explicit ToastView(std::u32string text) : text_(std::move(text)) {
    }

    void render(view::Frame &f, core::Rect area) const override {
      if (area.empty()) {
        return;
      }

      auto label =
          ui::label(text_, 0x88C0D0, view::layout::AlignH::Center,
                    view::layout::AlignV::Center, true);
      // Toast is a compact card pinned near the top-right edge.
      auto panel = view::PanelView::card(
          &label, 0x88C0D0, U'*', view::layout::Insets::hv(2, 1));

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

  // Compose the full dashboard layout for the current frame.
  void render_ui(view::Frame &frame, core::coord_t phase, int focus) {
    using namespace glyph;

    // Theme colors.
    const auto active_color = 0xE5E9F0;
    const auto nord_blue    = 0x88C0D0;

    // Background layer.
    BackgroundView bg(phase);
    const auto     card_base = view::PanelStyle::card(nord_blue);

    // Header title and panel.
    auto title = ui::label(U"Glyph - Aurora", nord_blue,
                           view::layout::AlignH::Center,
                           view::layout::AlignV::Center, true);

    auto header = view::PanelView::header(&title, nord_blue);

    // Main hero card (left column).
    auto hero =
        ui::label(U"NOW PLAYING\n"
                  U"Neon Drift - 3:42\n"
                  U"Ambient / 124 bpm\n\n"
                  U"Queue: 12 tracks",
                  active_color, view::layout::AlignH::Left,
                  view::layout::AlignV::Top);
    auto hero_panel = view::PanelView::card(
        &hero, focus == 0 ? active_color : nord_blue, U'#');

    // Status card (right column, top).
    auto stats =
        ui::label(U"ACTIVE\n"
                  U"- 24 nodes\n"
                  U"- 3.2ms\n"
                  U"- 99.99%",
                  0xA3BE8C, view::layout::AlignH::Left,
                  view::layout::AlignV::Top);
    auto stats_panel = view::PanelView::card(
        &stats, focus == 1 ? active_color : core::Color{0xA3BE8C});

    // Alert card (right column, bottom).
    auto alerts =
        ui::label(U"ALERTS\n"
                  U"- None\n"
                  U"- Systems nominal",
                  0xD08770, view::layout::AlignH::Left,
                  view::layout::AlignV::Top);
    auto alerts_panel = view::PanelView::card(
        &alerts, focus == 2 ? active_color : core::Color{0xD08770});

    // Right column stack with fixed heights.
    auto right_stack = view::VStack(
        {
            view::Fixed(stats_panel, 6),
            view::Fixed(alerts_panel, 5),
        },
        1);

    // Body layout: hero left, stats/alerts right.
    auto body = view::HStack(
        {
            view::Flex(hero_panel, 2),
            view::Flex(right_stack, 1),
        },
        2);

    // Footer hint line.
    auto footer =
        ui::label(U"Press Q to exit - Tab to cycle focus - Built with Glyph",
                  active_color, view::layout::AlignH::Center,
                  view::layout::AlignV::Center);

    // Overall vertical layout: header, body, footer.
    auto layout = view::VStack(
        {
            view::Fixed(header, 4),
            view::Flex(body, 1),
            view::Fixed(footer, 2),
        },
        1);

    // Overlay toast on top of background + layout.
    ToastView toast(U"Connected");

    auto root = view::ZStackView({&bg, &layout, &toast});
    root.render(frame, frame.bounds());
  }

} // namespace

int main() {
  using namespace glyph;
  using namespace std::chrono_literals;

  // Terminal app and raw input setup.
  render::TerminalApp  app{std::cout};
  input::WinInput      input{};
  input::InputGuard    input_guard(input, input::InputMode::Raw);
  render::TerminalSize last{};
  core::Size           last_size{};
  bool                 first = true;
  core::coord_t        phase = 0;
  int                  focus = 0;
  bool                 skip_sleep = false;
  bool                 should_quit = false;
  const auto           frame_step = 16ms;
  auto                 next_frame = std::chrono::steady_clock::now();

  for (;;) {
    // Query terminal size and fall back to a sane default.
    const auto term = app.size();
    const auto size =
        core::Size{term.valid ? term.cols : 80, term.valid ? term.rows : 24};

    if (size.w <= 0 || size.h <= 0) {
      std::this_thread::sleep_for(50ms);
      continue;
    }

    // Track terminal resize to avoid redundant work.
    if (term.valid && (last.cols != term.cols || last.rows != term.rows ||
                       last.valid != term.valid)) {
      last = term;
    }

    // Input handling: exit and focus cycling.
    skip_sleep = false;
    bool had_input = false;
    for (;;) {
      auto ev = input.poll();
      if (!std::holds_alternative<core::KeyEvent>(ev)) {
        break;
      }
      had_input = true;
      const auto &key = std::get<core::KeyEvent>(ev);
      if (key.code == core::KeyCode::Esc) {
        should_quit = true;
        break;
      }
      if (key.code == core::KeyCode::Char &&
          (key.ch == U'q' || key.ch == U'Q')) {
        should_quit = true;
        break;
      }
      if (key.code == core::KeyCode::Tab ||
          (key.code == core::KeyCode::Char &&
           (key.ch == U'\t' || key.ch == U'T'))) {
        focus = (focus + 1) % 3;
        first = true;
        skip_sleep = true;
        // Force a full redraw to avoid diff artifacts during focus changes.
        app.reset_renderer();
      }
    }

    if (had_input) {
      skip_sleep = true;
    }

    if (should_quit) {
      break;
    }

    // Advance the background phase for subtle animation.
    phase = core::coord_t((phase + 1) % 7);

    // Build and render the frame.
    view::Frame frame{size};
    render_ui(frame, phase, focus);
    app.render(frame);

    last_size = size;
    first     = false;
    // Fixed cadence to keep CPU use reasonable.
    const auto now = std::chrono::steady_clock::now();
    if (skip_sleep) {
      next_frame = now;
      continue;
    }

    if (now >= next_frame) {
      next_frame = now + frame_step;
      continue;
    }

    std::this_thread::sleep_until(next_frame);
    next_frame += frame_step;
  }

  return 0;
}

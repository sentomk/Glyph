// samples/basic/components_demo.cpp
//
// Component demo: shows FillView, LabelView, PanelView, BarView, and TableView.

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/core/style.h"
#include "glyph/render/terminal.h"
#include "glyph/input/input_guard.h"
#include "glyph/input/win32/win_input.h"
#include "glyph/view/frame.h"
#include "glyph/view/view.h"

#include "glyph/view/components/panel.h"
#include "glyph/view/components/stack.h"
#include "glyph/view/components/fill.h"
#include "glyph/view/components/label.h"
#include "glyph/view/components/bar.h"
#include "glyph/view/components/table.h"
#include "glyph/view/layout/box.h"
#include "glyph/view/layout/inset.h"
#include "glyph/view/layout/scroll.h"

namespace {

  void render_demo(glyph::view::Frame &frame,
                   const std::vector<glyph::view::TableView::Row> &rows,
                   glyph::view::layout::ScrollModel &scroll) {
    using namespace glyph;

    // Step 1: Clear the frame with a visible background.
    frame.fill(core::Cell::from_char(U'.'));

    // Step 2: Define the content area with a uniform inset.
    const auto area =
        view::layout::inset_rect(frame.bounds(), view::layout::Insets::all(1));
    if (area.empty()) {
      return;
    }

    view::layout::BoxItem cols[] = {
        {-1, 1},
        {-1, 1},
        {-1, 1},
    };
    const auto col_rects =
        view::layout::layout_box(view::layout::Axis::Horizontal, area, cols, 1);
    if (col_rects.rects.size() < 3) {
      return;
    }

    const auto left_area = col_rects.rects[0];
    const auto right_area = col_rects.rects[2];

    // Step 3: Build left column content (fill + styled title).
    view::FillView  fill_left(core::Cell::from_char(U'L'));
    view::LabelView title(U"Glyph Components");
    title.set_align(view::layout::AlignH::Center, view::layout::AlignV::Top);
    {
      core::Style s = core::Style::with_fg(core::Style::rgb(255, 215, 0));
      s.attrs       = core::Style::AttrBold;
      title.set_cell(core::Cell::from_char(U' ', s));
    }
    // Step 4: Build center column content (panel with fill + border).
    view::FillView fill_mid(core::Cell::from_char(U' '));

    // Step 5: Build right column content (bar + table + ellipsis label).
    const auto bar_bg = core::Cell::from_char(
        U' ', core::Style{}.bg(0x2E3440));
    auto bar_left =
        view::LabelView(U"Status: OK")
            .set_align(view::layout::AlignH::Left,
                       view::layout::AlignV::Center)
            .set_cell(core::Cell::from_char(U' ', core::Style{}.fg(0xECEFF4)));
    auto bar_right =
        view::LabelView(U"F1 Help")
            .set_align(view::layout::AlignH::Right,
                       view::layout::AlignV::Center)
            .set_cell(core::Cell::from_char(U' ', core::Style{}.fg(0xECEFF4)));
    auto bar_overlay = view::ZStackView({&bar_left, &bar_right});
    view::BarView top_bar(&bar_overlay, bar_bg);

    std::vector<view::TableView::Column> columns = {
        {U"Name", 8, 1, view::layout::AlignH::Left},
        {U"State", -1, 1, view::layout::AlignH::Left},
        {U"Value", 6, 1, view::layout::AlignH::Right},
    };
    view::TableView table(std::move(columns));
    table.set_header_cell(core::Cell::from_char(
        U' ', core::Style{}.fg(0xEBCB8B).bold()));
    table.set_cell(core::Cell::from_char(
        U' ', core::Style{}.fg(0xD8DEE9)));
    const auto right_inner =
        view::layout::inset_rect(right_area, view::layout::Insets::all(1));
    view::layout::BoxItem right_items[] = {
        {1, 0},
        {-1, 1},
        {2, 0},
    };
    const auto right_layout = view::layout::layout_box(
        view::layout::Axis::Vertical, right_inner, right_items, 1);
    const auto table_height =
        right_layout.rects.size() >= 2 ? right_layout.rects[1].size.h : 0;

    scroll.set_content(static_cast<core::coord_t>(rows.size()));
    scroll.set_viewport(table_height);

    std::vector<view::TableView::Row> visible_rows;
    const auto start = std::max<core::coord_t>(0, scroll.visible_start());
    const auto end = std::min<core::coord_t>(
        static_cast<core::coord_t>(rows.size()), scroll.visible_end());
    for (core::coord_t i = start; i < end; ++i) {
      visible_rows.push_back(rows[static_cast<std::size_t>(i)]);
    }
    table.set_rows(std::move(visible_rows));

    view::LabelView bottom(
        U"Ellipsis: The quick brown fox jumps over the lazy dog.");
    bottom.set_align(view::layout::AlignH::Left, view::layout::AlignV::Top);
    bottom.set_wrap(false);
    bottom.set_ellipsis(true);

    auto right_content = view::VStack(
        {
            view::Fixed(top_bar, 1),
            view::Flex(table, 1),
            view::Fixed(bottom, 2),
        },
        1);

    // Step 6: Wrap center/right content in panels with their own styling.
    view::PanelView mid_panel(&fill_mid);
    mid_panel.set_fill(core::Cell::from_char(U' '));
    mid_panel.set_border(core::Cell::from_char(U'#'));
    mid_panel.set_padding(view::layout::Insets::all(1));
    mid_panel.set_draw_fill(true);
    mid_panel.set_draw_border(true);

    view::PanelView right_panel(&right_content);
    right_panel.set_fill(core::Cell::from_char(U' '));
    right_panel.set_border(core::Cell::from_char(U'='));
    right_panel.set_padding(view::layout::Insets::all(1));
    right_panel.set_draw_fill(true);
    right_panel.set_draw_border(true);

    // Step 7: Lay out three columns horizontally and render them.
    auto layout = view::HStack(
        {
            view::StackChild{.view = &fill_left, .weight = 1},
            view::StackChild{.view = &mid_panel, .weight = 1},
            view::StackChild{.view = &right_panel, .weight = 1},
        },
        1);

    layout.render(frame, area);

    // Step 8: Place the title inside the left column only.
    const auto title_area = view::layout::inset_rect(
        left_area, view::layout::Insets::all(1));
    title.render(frame, title_area);
  }

} // namespace

int main() {
  using namespace glyph;
  using namespace std::chrono_literals;

  render::TerminalApp app{std::cout};
  input::WinInput     input{};
  input::InputGuard   guard(input, input::InputMode::Raw);
  bool                should_quit = false;
  bool                needs_render = true;
  core::Size           last_size{};

  std::vector<view::TableView::Row> rows = {
      {U"CPU", U"Active", U"24%"},
      {U"Mem", U"Normal", U"5.1G"},
      {U"IO", U"Idle", U"0.2G"},
      {U"Net", U"Active", U"1.2G"},
      {U"GPU", U"Idle", U"3%"},
  };
  view::layout::ScrollModel scroll;
  scroll.set_content(static_cast<core::coord_t>(rows.size()));
  scroll.set_viewport(1);

  for (;;) {
    const auto term = app.size();
    const auto size =
        core::Size{term.valid ? term.cols : 60, term.valid ? term.rows : 14};

    if (size.w <= 0 || size.h <= 0) {
      std::this_thread::sleep_for(50ms);
      continue;
    }

    if (size != last_size) {
      needs_render = true;
      last_size = size;
    }

    for (;;) {
      auto ev = input.poll();
      if (!std::holds_alternative<core::KeyEvent>(ev)) {
        break;
      }

      const auto &key = std::get<core::KeyEvent>(ev);
      if (key.code == core::KeyCode::Char &&
          (key.ch == U'q' || key.ch == U'Q')) {
        should_quit = true;
        break;
      }

      const auto prev = scroll.offset;
      if (key.code == core::KeyCode::Up) {
        scroll.scroll_by(-1);
      }
      else if (key.code == core::KeyCode::Down) {
        scroll.scroll_by(1);
      }
      else if (key.code == core::KeyCode::PageUp) {
        scroll.scroll_by(-scroll.viewport);
      }
      else if (key.code == core::KeyCode::PageDown) {
        scroll.scroll_by(scroll.viewport);
      }
      else if (key.code == core::KeyCode::Home) {
        scroll.scroll_to_start();
      }
      else if (key.code == core::KeyCode::End) {
        scroll.scroll_to_end();
      }

      if (scroll.offset != prev) {
        needs_render = true;
      }
    }

    if (should_quit) {
      break;
    }

    if (needs_render) {
      view::Frame frame{size};
      render_demo(frame, rows, scroll);
      app.render(frame);
      needs_render = false;
    }

    std::this_thread::sleep_for(16ms);
  }

  return 0;
}

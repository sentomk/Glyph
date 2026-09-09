// samples/ui/selection_demo.cpp
//
// TUI selection + copy demo (tmux copy-mode style).
// Shows: full-screen mode, mouse-drag selection with reverse-video
// highlight, clipboard copy, resize handling, status feedback.

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "glyph/core/event.h"
#include "glyph/input/input.h"
#include "glyph/input/input_guard.h"
#include "glyph/platform/clipboard.h"
#include "glyph/render/ansi/ansi_renderer.h"
#include "glyph/render/terminal.h"
#include "glyph/view/components/status_line.h"
#include "glyph/view/frame.h"
#include "glyph/view/selection.h"
#include "glyph/view/text.h"

namespace {

using namespace glyph;

const std::vector<std::string> kLines = {
    "Glyph selection demo — drag with the mouse to select text.",
    "",
    "src/render/ansi/ansi_renderer.cpp:116: emit_utf8()",
    "src/input/vt_decoder.cpp:74: step_esc() decodes Alt+key",
    "include/glyph/view/selection.h: SelectionModel",
    "宽字符也能选中：中文行、emoji 🚀、CJK 混排 English",
    "",
    "y / Enter  copy selection to the system clipboard",
    "Esc        clear selection (press again to quit)",
    "q          quit",
};

} // namespace

int main() {
  render::FullScreenGuard guard;

  auto input = input::make_default_input();
  input::InputGuard modes(*input,
                          input::InputMode::Raw | input::InputMode::Mouse);

  render::AnsiRenderer renderer{std::cout};

  core::Size size = render::terminal_frame_size({80, 24});

  view::SelectionModel selection;
  view::StatusLineView status;
  core::Style          ok_style{};
  ok_style.fg(0xA3BE8C);

  std::string note;

  auto rebuild_status = [&]() {
    std::vector<view::StatusLineView::Segment> segments;
    if (!note.empty()) {
      segments.push_back({note + "  ·  ", ok_style});
    }
    segments.push_back(
        {"glyph  ·  drag to select · y copies · Esc clears · q quits",
         core::Style{}});
    status.set_segments(std::move(segments));
  };

  bool        running = true;
  view::Frame frame{size};

  while (running) {
    // Immediate-mode repaint: text first (overwrites any reverse-video
    // cells from the previous frame), then the current selection. The
    // ANSI renderer is diff-based, so an unchanged paint emits nothing.
    frame = view::Frame{size};
    for (std::size_t i = 0;
         i < kLines.size() && static_cast<core::coord_t>(i) < size.h - 1;
         ++i) {
      view::draw_text(frame, {0, static_cast<core::coord_t>(i)},
                      kLines[i]);
    }
    rebuild_status();
    selection.highlight(frame);
    status.render(frame, core::Rect{0, size.h - 1, size.w, 1});
    renderer.render(frame);

    const core::Event ev = input->poll();
    if (std::holds_alternative<std::monostate>(ev)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    if (const auto *key = std::get_if<core::KeyEvent>(&ev)) {
      if (key->code == core::KeyCode::Esc) {
        if (selection.active()) {
          selection.clear();
          note.clear();
        } else {
          running = false;
        }
      } else if (key->code == core::KeyCode::Char &&
                 key->ch == U'q' && key->mods == core::Mod::None) {
        running = false;
      } else if ((key->code == core::KeyCode::Enter ||
                  (key->code == core::KeyCode::Char &&
                   key->ch == U'y' && key->mods == core::Mod::None)) &&
                 selection.active()) {
        const std::string text = selection.extract(frame);
        if (!text.empty() &&
            platform::copy_to_clipboard(text)) {
          note = "copied " + std::to_string(text.size()) + " bytes";
        } else {
          note = "copy failed (empty selection or no clipboard tool)";
        }
      }
    }

    if (const auto *mouse = std::get_if<core::MouseEvent>(&ev)) {
      if (mouse->action == core::MouseAction::Down &&
          mouse->button == core::MouseButton::Left) {
        selection.begin(mouse->pos);
        note.clear();
      } else if ((mouse->action == core::MouseAction::Drag ||
                  mouse->action == core::MouseAction::Move) &&
                 selection.active()) {
        selection.extend(mouse->pos);
      }
    }

    if (const auto *resize = std::get_if<core::ResizeEvent>(&ev)) {
      if (resize->size.w > 0 && resize->size.h > 0) {
        size = resize->size;
      }
    }
  }

  return 0;
}

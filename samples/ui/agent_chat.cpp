// samples/ui/agent_chat.cpp
//
// TUI AI Agent chat demo.
// Shows: streaming text, scrollable (wrapped) message history via
// ScrollRegionView, mouse-drag selection with drag-at-edge scroll-back,
// wheel scrolling, clipboard copy, text input, spinner.

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "glyph/core/cell.h"
#include "glyph/core/color.h"
#include "glyph/core/event.h"
#include "glyph/core/geometry.h"
#include "glyph/core/style.h"
#include "glyph/input/input.h"
#include "glyph/input/input_guard.h"
#include "glyph/platform/clipboard.h"
#include "glyph/render/ansi/ansi_renderer.h"
#include "glyph/render/terminal.h"
#include "glyph/view/components/fill.h"
#include "glyph/view/components/label.h"
#include "glyph/view/components/scroll_region.h"
#include "glyph/view/components/status_line.h"
#include "glyph/view/components/text_input.h"
#include "glyph/view/frame.h"


namespace {

using namespace glyph;
using namespace std::chrono_literals;

// Nord-inspired palette.
constexpr core::Color kBgDark    = 0x2E3440;
constexpr core::Color kBgPanel   = 0x3B4252;
constexpr core::Color kFgNormal  = 0xD8DEE9;
constexpr core::Color kFgBright  = 0xECEFF4;
constexpr core::Color kAccentBot = 0x88C0D0;
constexpr core::Color kAccentUsr = 0xA3BE8C;
constexpr core::Color kDimmed    = 0x4C566A;
constexpr core::Color kWarn      = 0xEBCB8B;

// Simulated AI response that streams token-by-token.
struct StreamState {
  std::u32string full_response;
  std::size_t    chars_shown = 0;
  bool           active      = false;
  int            think_ticks = 0;

  void start(std::u32string response) {
    full_response = std::move(response);
    chars_shown   = 0;
    active        = true;
    think_ticks   = 8;
  }

  bool tick() {
    if (!active)
      return false;
    if (think_ticks > 0) {
      --think_ticks;
      return true;
    }
    if (chars_shown < full_response.size()) {
      const std::size_t step = 2 + (chars_shown % 3);
      chars_shown = std::min(chars_shown + step, full_response.size());
      return true;
    }
    active = false;
    return false;
  }

  std::u32string visible_text() const {
    if (think_ticks > 0)
      return U"";
    return full_response.substr(0, chars_shown);
  }

  bool done() const {
    return !active;
  }
};

// Canned responses for the demo.
const std::u32string kResponses[] = {
    U"I can help you with that. Let me look at the code and find "
    U"the relevant files. The main entry point is in src/main.cpp "
    U"and it calls into the render pipeline.",

    U"Here's what I found:\n\n"
    U"1. The diff algorithm uses FNV-1a hashing per line\n"
    U"2. Only dirty lines get re-rendered\n"
    U"3. Style changes are tracked to minimize SGR sequences\n\n"
    U"This gives you efficient incremental updates.",

    U"Looking at the architecture, I'd suggest adding a "
    U"RichText model that holds styled spans. Each span would "
    U"carry a Style and a range. The LabelView could then "
    U"accept a RichText instead of plain u32string.",

    U"Done. I've refactored the input loop to use a state "
    U"machine pattern. The event dispatch is now cleaner "
    U"and each handler returns a Command variant.",
};

const char32_t *kSpinner[] = {U"|", U"/", U"-", U"\\"};

void append_utf8(std::string &out, char32_t cp) {
  if (cp < 0x80) {
    out.push_back(char(cp));
  } else if (cp < 0x800) {
    out.push_back(char(0xC0 | (cp >> 6)));
    out.push_back(char(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    out.push_back(char(0xE0 | (cp >> 12)));
    out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(char(0x80 | (cp & 0x3F)));
  } else {
    out.push_back(char(0xF0 | (cp >> 18)));
    out.push_back(char(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(char(0x80 | (cp & 0x3F)));
  }
}

std::string to_utf8(const std::u32string &s) {
  std::string out;
  for (char32_t c : s) {
    append_utf8(out, c);
  }
  return out;
}

} // namespace

int main() {
  using namespace glyph;
  using namespace std::chrono_literals;

  render::FullScreenGuard guard;

  auto input_owner = input::make_default_input();
  auto &in         = *input_owner;
  input::InputGuard modes(in, input::InputMode::Raw |
                                   input::InputMode::Mouse);

  render::AnsiRenderer renderer{std::cout};
  core::Size           size = render::terminal_frame_size({80, 24});

  StreamState    stream;
  view::TextInputView input_field;
  input_field.set_cell(
      core::Cell::from_char(U' ', core::Style{}.fg(kFgBright).bg(kBgPanel)));
  input_field.set_placeholder(U"Type a message...");
  input_field.set_placeholder_cell(core::Cell::from_char(
      U' ', core::Style{}.fg(kFgNormal).bg(kBgPanel).dim()));

  // Incremental scrollback: push once per logical line and grow the
  // streaming line with replace_last_line — no clear/rebuild, so the
  // content-anchored selection and the reader's scroll position stay
  // stable while responses stream.
  view::ScrollRegionView logs{1000};

  std::string note;
  int         spinner_phase = 0;
  int         response_idx  = 0;

  logs.push_line(
      "Hello! I'm an AI agent running in your terminal. Ask me "
      "anything about this codebase. (Glyph TUI demo.)",
      core::Style{}.fg(kAccentBot));
  logs.push_line("", core::Style{});
  logs.push_line(
      "Scroll with the wheel; drag to select (drag past the top edge "
      "to reach earlier output — the selection follows the content, so "
      "everything you swept is copied); y copies; Enter sends.",
      core::Style{}.fg(kAccentBot));
  logs.push_line("", core::Style{});

  auto layout = [&]() {
    const core::coord_t header_h = 1, sep_h = 1, input_h = 3, status_h = 1;
    const core::coord_t msg_h = core::coord_t(
        size.h - header_h - sep_h - input_h - status_h);
    struct {
      core::Rect header, messages, sep, input, status;
    } r;
    r.header = {0, 0, size.w, header_h};
    r.messages = {0, header_h, size.w, msg_h};
    r.sep = {0, core::coord_t(r.messages.bottom()), size.w, sep_h};
    r.input = {0, core::coord_t(r.sep.bottom()), size.w, input_h};
    r.status = {0, core::coord_t(r.input.bottom()), size.w, status_h};
    return r;
  };

  bool running = true;
  auto last_tick = std::chrono::steady_clock::now();

  while (running) {
    const auto l = layout();
    if (l.messages.size.h <= 2 || size.w <= 4) {
      std::this_thread::sleep_for(50ms);
      continue;
    }

    // -- Paint (immediate mode; the diff renderer skips no-op frames) --
    view::Frame frame{size};
    frame.fill(core::Cell::from_char(U' ', core::Style{}.bg(kBgDark)));

    view::FillView header_bg(
        core::Cell::from_char(U' ', core::Style{}.bg(kBgPanel)));
    header_bg.render(frame, l.header);
    auto header = view::LabelView(U" Glyph Agent Chat")
                      .set_cell(core::Cell::from_char(
                          U' ', core::Style{}.fg(kFgBright).bg(kBgPanel).bold()));
    header.render(frame, l.header);

    const core::Rect msg_area{1, l.messages.top() + 1,
                              core::coord_t(size.w - 2),
                              core::coord_t(l.messages.size.h - 2)};
    logs.render(frame, msg_area);

    for (core::coord_t x = l.sep.left(); x < l.sep.right(); ++x) {
      frame.set({x, l.sep.top()},
                core::Cell::from_char(U'-', core::Style{}.fg(kDimmed)));
    }

    view::FillView input_bg(
        core::Cell::from_char(U' ', core::Style{}.bg(kBgPanel)));
    input_bg.render(frame, l.input);
    const core::Rect input_content{1, l.input.top() + 1,
                                   core::coord_t(size.w - 2), 1};
    auto prompt = view::LabelView(U"> ")
                      .set_cell(core::Cell::from_char(
                          U' ', core::Style{}.fg(kFgBright).bg(kBgPanel)));
    prompt.render(frame, input_content);
    core::Rect field_area = input_content;
    field_area.origin.x = core::coord_t(field_area.origin.x + 2);
    field_area.size.w   = core::coord_t(field_area.size.w - 2);
    input_field.set_show_cursor(!stream.active);
    input_field.render(frame, field_area);

    view::StatusLineView status;
    std::vector<view::StatusLineView::Segment> segs;
    if (!note.empty()) {
      segs.push_back({note + "  ·  ", core::Style{}.fg(kAccentUsr)});
    }
    std::string hint = stream.active ? "streaming" : "ready";
    hint += "  ·  Enter:send  y:copy sel  wheel/drag:scroll  Esc:clear/quit";
    segs.push_back({hint, core::Style{}.fg(kFgNormal)});
    status.set_segments(std::move(segs));
    view::FillView status_bg(
        core::Cell::from_char(U' ', core::Style{}.bg(0x434C5E)));
    status_bg.render(frame, l.status);
    status.render(frame, l.status);

    renderer.render(frame);

    // -- Events --
    for (core::Event ev = in.poll();
         !std::holds_alternative<std::monostate>(ev) && running;
         ev = in.poll()) {

      if (const auto *key = std::get_if<core::KeyEvent>(&ev)) {
        if (key->code == core::KeyCode::Esc) {
          if (logs.selection_active()) {
            logs.select_clear();
            note.clear();
          } else {
            running = false;
          }
        } else if (key->code == core::KeyCode::Char &&
                   key->ch == U'y' && key->mods == core::Mod::None &&
                   logs.selection_active()) {
          const std::string text = logs.extract_selection();
          if (!text.empty() && platform::copy_to_clipboard(text)) {
            note = "copied " + std::to_string(text.size()) + " bytes";
          } else {
            note = "copy failed";
          }
        } else if (stream.active) {
          continue;
        } else if (key->code == core::KeyCode::Enter &&
                   !input_field.empty()) {
          logs.push_line("> " + to_utf8(input_field.text()),
                         core::Style{}.fg(kAccentUsr));
          logs.push_line("", core::Style{});
          input_field.clear();
          stream.start(kResponses[response_idx % 4]);
          ++response_idx;
          logs.push_line("  ...", core::Style{}.fg(kWarn)); // placeholder
          logs.scroll_to_bottom(); // sending implies wanting the latest
        } else if (input_field.handle_key(*key)) {
          // editing; nothing else to do
        }
      }

      if (const auto *m = std::get_if<core::MouseEvent>(&ev)) {
        const bool in_messages =
            m->pos.x >= msg_area.left() && m->pos.x < msg_area.right() &&
            m->pos.y >= msg_area.top() && m->pos.y < msg_area.bottom();

        if (m->action == core::MouseAction::Scroll && in_messages) {
          logs.on_mouse(*m); // wheel: scrollback (issue #5)
          continue;
        }
        if (!in_messages) {
          continue;
        }

        // Clamp the tracked point into the message area.
        auto clamp = [&](core::Point p) {
          p.x = std::clamp(p.x, msg_area.left(),
                           core::coord_t(msg_area.right() - 1));
          p.y = std::clamp(p.y, msg_area.top(),
                           core::coord_t(msg_area.bottom() - 1));
          return p;
        };

        if (m->action == core::MouseAction::Down &&
            m->button == core::MouseButton::Left) {
          logs.select_begin(clamp(m->pos));
          note.clear();
        } else if ((m->action == core::MouseAction::Drag ||
                    m->action == core::MouseAction::Move) &&
                   logs.selection_active()) {
          // Drag past the top edge pulls older output into view —
          // same effect as the wheel, driven by the selection gesture
          // (tmux/terminal behavior). The selection is content
          // anchored, so rows swept out of view mid-drag are still
          // captured by extract_selection().
          if (m->pos.y <= msg_area.top()) {
            logs.scroll_up(1);
            logs.select_extend(clamp(m->pos));
          } else if (m->pos.y >= msg_area.bottom() - 1) {
            logs.scroll_down(1);
            logs.select_extend(clamp(m->pos));
          } else {
            logs.select_extend(clamp(m->pos));
          }
        }
      }

      if (const auto *rz = std::get_if<core::ResizeEvent>(&ev)) {
        if (rz->size.w > 0 && rz->size.h > 0) {
          size = rz->size;
        }
      }
    }

    // -- Stream tick (~30 chars/s) --
    const auto now = std::chrono::steady_clock::now();
    if (now - last_tick >= 33ms) {
      last_tick = now;
      ++spinner_phase;
      if (stream.active) {
        stream.tick();
        if (stream.think_ticks > 0) {
          std::string thinking = "  ";
          thinking += char(*kSpinner[spinner_phase % 4]);
          thinking += " thinking...";
          logs.replace_last_line(thinking, core::Style{}.fg(kWarn));
        } else if (stream.done()) {
          logs.replace_last_line("  " + to_utf8(stream.full_response),
                                 core::Style{}.fg(kAccentBot));
          logs.push_line("", core::Style{}); // trailing separator
        } else {
          logs.replace_last_line("  " + to_utf8(stream.visible_text()),
                                 core::Style{}.fg(kAccentBot));
        }
      }
    }

    std::this_thread::sleep_for(10ms);
  }

  return 0;
}

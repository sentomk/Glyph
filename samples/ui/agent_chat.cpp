// samples/ui/agent_chat.cpp
//
// TUI AI Agent chat demo.
// Shows: streaming text, scrollable message history, panels, input, spinner.

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "glyph/core/cell.h"
#include "glyph/core/color.h"
#include "glyph/core/event.h"
#include "glyph/core/geometry.h"
#include "glyph/core/style.h"
#include "glyph/input/input_guard.h"
#include "glyph/input/input.h"
#include "glyph/render/terminal.h"
#include "glyph/view/components/fill.h"
#include "glyph/view/components/label.h"
#include "glyph/view/components/text_input.h"
#include "glyph/view/frame.h"
#include "glyph/view/layout/align.h"
#include "glyph/view/layout/inset.h"
#include "glyph/view/view.h"

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

// A single chat message.
struct Message {
  enum Role { User, Assistant };
  Role           role;
  std::u32string text;
  bool           streaming = false;
};

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
    if (!active) return false;
    if (think_ticks > 0) {
      --think_ticks;
      return true;
    }
    if (chars_shown < full_response.size()) {
      std::size_t step = 2 + (chars_shown % 3);
      chars_shown = std::min(chars_shown + step, full_response.size());
      return true;
    }
    active = false;
    return false;
  }

  std::u32string visible_text() const {
    if (think_ticks > 0) return U"";
    return full_response.substr(0, chars_shown);
  }

  bool done() const { return !active; }
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

// Spinner frames.
const char32_t *kSpinner[] = {
    U"|", U"/", U"-", U"\\",
};

// Render a single message bubble.
class MessageView final : public view::View {
public:
  MessageView(const Message &msg, core::coord_t width)
      : msg_(msg), width_(width) {}

  void render(view::Frame &f, core::Rect area) const override {
    if (area.empty()) return;

    const bool is_user = (msg_.role == Message::User);
    const auto accent  = is_user ? kAccentUsr : kAccentBot;
    const auto prefix  = is_user ? U"> " : U"  ";

    auto label = view::LabelView(prefix + msg_.text)
                     .set_align(view::layout::AlignH::Left,
                                view::layout::AlignV::Top)
                     .set_wrap_mode(view::LabelView::WrapMode::Word)
                     .set_cell(core::Cell::from_char(
                         U' ', core::Style{}.fg(accent)));

    label.render(f, area);
  }

  static core::coord_t estimate_height(const std::u32string &text,
                                       core::coord_t width) {
    if (width <= 2) return 1;
    core::coord_t w   = 0;
    core::coord_t lines = 1;
    for (char32_t ch : text) {
      if (ch == U'\n') {
        ++lines;
        w = 0;
        continue;
      }
      ++w;
      if (w >= width - 2) {
        ++lines;
        w = 0;
      }
    }
    return lines;
  }

private:
  const Message &msg_;
  core::coord_t  width_;
};

// Render the full chat UI.
void render_ui(view::Frame &frame, const std::vector<Message> &messages,
               const StreamState &stream, int spinner_phase,
               view::TextInputView &input_field) {
  const auto bounds = frame.bounds();
  if (bounds.empty()) return;

  // Background fill.
  frame.fill(core::Cell::from_char(U' ', core::Style{}.bg(kBgDark)));

  // Layout: header(1) + messages(flex) + separator(1) + input(3) + status(1)
  const core::coord_t header_h = 1;
  const core::coord_t sep_h    = 1;
  const core::coord_t input_h  = 3;
  const core::coord_t status_h = 1;
  const core::coord_t msg_h =
      bounds.size.h - header_h - sep_h - input_h - status_h;

  if (msg_h <= 0) return;

  const auto header_area = core::Rect{
      bounds.origin, core::Size{bounds.size.w, header_h}};
  const auto msg_area = core::Rect{
      {bounds.origin.x, core::coord_t(bounds.origin.y + header_h)},
      core::Size{bounds.size.w, msg_h}};
  const auto sep_area = core::Rect{
      {bounds.origin.x, core::coord_t(msg_area.bottom())},
      core::Size{bounds.size.w, sep_h}};
  const auto input_area = core::Rect{
      {bounds.origin.x, core::coord_t(sep_area.bottom())},
      core::Size{bounds.size.w, input_h}};
  const auto status_area = core::Rect{
      {bounds.origin.x, core::coord_t(input_area.bottom())},
      core::Size{bounds.size.w, status_h}};

  // -- Header --
  auto header_label =
      view::LabelView(U" Glyph Agent Chat")
          .set_align(view::layout::AlignH::Left,
                     view::layout::AlignV::Center)
          .set_cell(core::Cell::from_char(
              U' ', core::Style{}.fg(kFgBright).bg(kBgPanel).bold()));
  // Fill header background.
  view::FillView header_bg(
      core::Cell::from_char(U' ', core::Style{}.bg(kBgPanel)));
  header_bg.render(frame, header_area);
  header_label.render(frame, header_area);

  // -- Messages area --
  const auto content_area = view::layout::inset_rect(
      msg_area, view::layout::Insets::hv(1, 0));
  const core::coord_t content_w = content_area.size.w;

  // Compute total height needed and render messages bottom-aligned.
  struct MsgLayout {
    std::size_t   idx;
    core::coord_t height;
  };
  std::vector<MsgLayout> layouts;
  core::coord_t total_h = 0;
  for (std::size_t i = 0; i < messages.size(); ++i) {
    auto h = MessageView::estimate_height(
        (messages[i].role == Message::User ? U"> " : U"  ") +
            messages[i].text,
        content_w);
    h = core::coord_t(h + 1); // gap between messages
    layouts.push_back({i, h});
    total_h = core::coord_t(total_h + h);
  }

  // Streaming indicator.
  if (stream.active && stream.think_ticks > 0) {
    total_h = core::coord_t(total_h + 2);
  }

  // Render from bottom up, scrolled to show latest.
  core::coord_t y = core::coord_t(content_area.bottom());

  // Thinking indicator at the bottom.
  if (stream.active && stream.think_ticks > 0) {
    y = core::coord_t(y - 1);
    std::u32string spinner_text = U"  ";
    spinner_text += kSpinner[spinner_phase % 4];
    spinner_text += U" thinking...";
    auto think_label =
        view::LabelView(spinner_text)
            .set_align(view::layout::AlignH::Left,
                       view::layout::AlignV::Top)
            .set_cell(core::Cell::from_char(
                U' ', core::Style{}.fg(kWarn).bg(kBgDark)));
    think_label.render(
        frame,
        core::Rect{{content_area.left(), y},
                   core::Size{content_w, 1}});
    y = core::coord_t(y - 1);
  }

  // Render messages in reverse so newest is at bottom.
  for (auto it = layouts.rbegin(); it != layouts.rend(); ++it) {
    y = core::coord_t(y - it->height);
    if (y >= content_area.bottom()) continue;
    if (core::coord_t(y + it->height) <= content_area.top()) break;

    core::Rect msg_rect{
        {content_area.left(), std::max(y, content_area.top())},
        core::Size{content_w,
                   std::min(it->height,
                            core::coord_t(content_area.bottom() - y))}};

    MessageView mv(messages[it->idx], content_w);
    mv.render(frame, msg_rect);
  }

  // -- Separator --
  auto sep_cell = core::Cell::from_char(U'-', core::Style{}.fg(kDimmed));
  for (core::coord_t x = sep_area.left(); x < sep_area.right(); ++x) {
    frame.set({x, sep_area.top()}, sep_cell);
  }

  // -- Input area --
  view::FillView input_bg(
      core::Cell::from_char(U' ', core::Style{}.bg(kBgPanel)));
  input_bg.render(frame, input_area);

  auto input_content = view::layout::inset_rect(
      input_area, view::layout::Insets::hv(1, 1));

  // Prompt prefix "> " drawn as a label; the editable field follows it.
  const core::coord_t prompt_w = 2;
  auto prompt_label =
      view::LabelView(U"> ")
          .set_align(view::layout::AlignH::Left, view::layout::AlignV::Top)
          .set_cell(core::Cell::from_char(
              U' ', core::Style{}.fg(kFgBright).bg(kBgPanel)));
  prompt_label.render(frame, input_content);

  auto field_area = input_content;
  field_area.origin.x = core::coord_t(field_area.origin.x + prompt_w);
  field_area.size.w   = core::coord_t(field_area.size.w - prompt_w);

  // Hide the caret while the assistant is streaming.
  input_field.set_show_cursor(!stream.active);
  input_field.render(frame, field_area);

  // -- Status bar --
  view::FillView status_bg(
      core::Cell::from_char(U' ', core::Style{}.bg(0x434C5E)));
  status_bg.render(frame, status_area);

  std::u32string status_left = U" Enter:send  Esc:quit";
  std::u32string status_right =
      stream.active ? U"streaming... " : U"ready ";
  auto sl = view::LabelView(status_left)
                .set_align(view::layout::AlignH::Left,
                           view::layout::AlignV::Center)
                .set_cell(core::Cell::from_char(
                    U' ', core::Style{}.fg(kFgNormal).bg(0x434C5E)));
  auto sr = view::LabelView(status_right)
                .set_align(view::layout::AlignH::Right,
                           view::layout::AlignV::Center)
                .set_cell(core::Cell::from_char(
                    U' ', core::Style{}.fg(kWarn).bg(0x434C5E)));
  sl.render(frame, status_area);
  sr.render(frame, status_area);
}

} // namespace

int main() {
  using namespace glyph;
  using namespace std::chrono_literals;

  render::TerminalApp app{std::cout};
  auto input_owner_ = glyph::input::make_default_input();
  auto &input = *input_owner_;
  input::InputGuard   guard(input, input::InputMode::Raw);

  std::vector<Message> messages;
  messages.push_back(
      {Message::Assistant,
       U"Hello! I'm an AI agent running in your terminal. "
       U"Ask me anything about this codebase. "
       U"(This is a Glyph TUI demo.)",
       false});

  StreamState    stream;
  view::TextInputView input_field;
  input_field.set_cell(core::Cell::from_char(
      U' ', core::Style{}.fg(kFgBright).bg(kBgPanel)));
  input_field.set_placeholder(U"Type a message...");
  input_field.set_placeholder_cell(core::Cell::from_char(
      U' ', core::Style{}.fg(kFgNormal).bg(kBgPanel).dim()));
  int            response_idx = 0;
  int            spinner_phase = 0;
  bool           should_quit = false;
  bool           needs_render = true;
  core::Size     last_size{};

  auto last_tick = std::chrono::steady_clock::now();

  for (;;) {
    const auto term = app.size();
    const auto size =
        core::Size{term.valid ? term.cols : 80, term.valid ? term.rows : 24};
    if (size.w <= 0 || size.h <= 0) {
      std::this_thread::sleep_for(50ms);
      continue;
    }

    if (size != last_size) {
      needs_render = true;
      last_size = size;
    }

    // Process input events.
    for (;;) {
      auto ev = input.poll();
      if (std::holds_alternative<std::monostate>(ev)) break;

      if (std::holds_alternative<core::KeyEvent>(ev)) {
        const auto &key = std::get<core::KeyEvent>(ev);

        if (key.code == core::KeyCode::Esc) {
          should_quit = true;
          break;
        }

        if (stream.active) continue;

        if (key.code == core::KeyCode::Enter && !input_field.empty()) {
          messages.push_back({Message::User, input_field.text(), false});
          const auto &resp =
              kResponses[response_idx % 4];
          ++response_idx;
          messages.push_back({Message::Assistant, U"", true});
          stream.start(resp);
          input_field.clear();
          needs_render = true;
          continue;
        }

        // Delegate editing (insert / delete / caret motion) to the field.
        if (input_field.handle_key(key)) {
          needs_render = true;
        }
      }
    }

    if (should_quit) break;

    // Tick streaming at ~30 char/s.
    auto now = std::chrono::steady_clock::now();
    if (now - last_tick >= 33ms) {
      last_tick = now;
      ++spinner_phase;
      if (stream.active) {
        stream.tick();
        if (!messages.empty() && messages.back().streaming) {
          messages.back().text = stream.visible_text();
          if (stream.done()) {
            messages.back().streaming = false;
          }
        }
        needs_render = true;
      }
    }

    // Render only when state changed.
    if (needs_render) {
      view::Frame frame{size};
      render_ui(frame, messages, stream, spinner_phase, input_field);
      app.render(frame);
      needs_render = false;
    }

    std::this_thread::sleep_for(16ms);
  }

  return 0;
}

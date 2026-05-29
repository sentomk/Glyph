// glyph/render/ansi/ansi_renderer.cpp
//
// ANSI renderer with dirty-line diff spans.
//
// Notes:
//   - Full redraw on first frame or size change.
//   - Incremental updates via diff spans on dirty lines.
//   - Styles emitted as SGR only when they change.

#include "glyph/render/ansi/ansi_renderer.h"

#include "glyph/core/diff.h"
#include "glyph/view/frame.h"
#include <ostream>
#include <sstream>

namespace glyph::render {

  AnsiRenderer::AnsiRenderer(std::ostream &out) noexcept : out_(out) {
  }

  void AnsiRenderer::reset() noexcept {
    prev_.resize(core::Size{0, 0});
    has_prev_        = false;
    has_prev_cursor_ = false;
  }

  // Clear the entire screen.
  static void ansi_clear(std::ostream &out) {
    out << "\x1b[2J";
  }

  // Move the cursor to home (1,1).
  static void ansi_home(std::ostream &out) {
    out << "\x1b[H";
  }

  // Reset all SGR attributes.
  static void ansi_reset(std::ostream &out) {
    out << "\x1b[0m";
  }

  // Move cursor to zero-based row/col.
  static void ansi_move(
      std::ostream &out, glyph::core::coord_t row, glyph::core::coord_t col) {
    out << "\x1b[" << (row + 1) << ";" << (col + 1) << "H";
  }

  // Enable/disable terminal line wrapping.
  static void ansi_wrap(std::ostream &out, bool enable) {
    out << (enable ? "\x1b[?7h" : "\x1b[?7l");
  }

  // Apply the frame's cursor hint to the hardware cursor. A visible hint
  // positions and shows the cursor (so an IME anchors its candidate window
  // correctly); otherwise the cursor stays hidden.
  static void ansi_apply_cursor(std::ostream                    &out,
                                const view::Frame::CursorHint &hint) {
    if (hint.visible) {
      ansi_move(out, hint.pos.y, hint.pos.x);
      out << "\x1b[?25h";
    } else {
      out << "\x1b[?25l";
    }
  }

  // Emit SGR for the given style (true-color + attributes).
  static void ansi_apply_style(std::ostream &out, const glyph::core::Style &s) {
    out << "\x1b[0";

    if (s.attrs & glyph::core::Style::AttrBold) {
      out << ";1";
    }
    if (s.attrs & glyph::core::Style::AttrDim) {
      out << ";2";
    }
    if (s.attrs & glyph::core::Style::AttrItalic) {
      out << ";3";
    }
    if (s.attrs & glyph::core::Style::AttrUnderline) {
      out << ";4";
    }
    if (s.attrs & glyph::core::Style::AttrBlink) {
      out << ";5";
    }
    if (s.attrs & glyph::core::Style::AttrStrike) {
      out << ";9";
    }

    if (s.fg_is_default()) {
      out << ";39";
    }
    else {
      const std::uint32_t rgb = s.fg_rgb;
      const std::uint32_t r   = (rgb >> 16) & 0xFFu;
      const std::uint32_t g   = (rgb >> 8) & 0xFFu;
      const std::uint32_t b   = rgb & 0xFFu;
      out << ";38;2;" << r << ";" << g << ";" << b;
    }

    if (s.bg_is_default()) {
      out << ";49";
    }
    else {
      const std::uint32_t rgb = s.bg_rgb;
      const std::uint32_t r   = (rgb >> 16) & 0xFFu;
      const std::uint32_t g   = (rgb >> 8) & 0xFFu;
      const std::uint32_t b   = rgb & 0xFFu;
      out << ";48;2;" << r << ";" << g << ";" << b;
    }

    out << "m";
  }

  // Encode a single codepoint as UTF-8 into the output stream.
  static void emit_utf8(std::ostream &out, char32_t cp) noexcept {
    if (cp == 0) {
      out << ' ';
      return;
    }
    if (cp < 0x80) {
      out << static_cast<char>(cp);
    } else if (cp < 0x800) {
      out << static_cast<char>(0xC0 | (cp >> 6));
      out << static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      out << static_cast<char>(0xE0 | (cp >> 12));
      out << static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      out << static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x110000) {
      out << static_cast<char>(0xF0 | (cp >> 18));
      out << static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
      out << static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      out << static_cast<char>(0x80 | (cp & 0x3F));
    } else {
      out << '?';
    }
  }

  // Render a single dirty span with style tracking.
  static void render_span(
      std::ostream                &out,
      glyph::core::ConstBufferView buf,
      glyph::core::DiffSpan        span,
      glyph::core::Style          &current,
      bool                        &has_current) {
    if (span.empty())
      return;

    ansi_move(out, span.y, span.x0);

    for (glyph::core::coord_t x = span.x0; x < span.x1; ++x) {
      const auto &cell = buf.at(x, span.y);

      if (cell.width == 0) {
        out << ' ';
        continue;
      }

      if (!has_current || cell.style != current) {
        ansi_apply_style(out, cell.style);
        current     = cell.style;
        has_current = true;
      }

      emit_utf8(out, cell.ch);

      if (cell.width == 2) {
        // The terminal advances two columns for a wide glyph on its own.
        // Skip the paired spacer cell without emitting anything, or the
        // terminal cursor would drift one column ahead of the model.
        ++x;
      }
    }
  }

  void AnsiRenderer::render(const view::Frame &frame) {
    if (frame.empty()) {
      if (has_prev_) {
        out_ << "\x1b[2J\x1b[H\x1b[0m";
        out_.flush();
        has_prev_ = false;
      }
      return;
    }

    const auto size = frame.size();
    const auto cur  = frame.view();

    std::ostringstream buf;

    // First frame or size change: full redraw.
    if (!has_prev_ || prev_.size() != size) {
      buf << "\x1b[H\x1b[0m";
      ansi_wrap(buf, false);

      glyph::core::Style current{};
      bool               has_current = false;

      for (glyph::core::coord_t y = 0; y < size.h; ++y) {
        for (glyph::core::coord_t x = 0; x < size.w; ++x) {
          const auto &cell = cur.at(x, y);

          if (cell.width == 0) {
            buf << ' ';
            continue;
          }

          if (!has_current || cell.style != current) {
            ansi_apply_style(buf, cell.style);
            current     = cell.style;
            has_current = true;
          }

          emit_utf8(buf, cell.ch);

          if (cell.width == 2) {
            // Wide glyph already occupies two terminal columns; just skip
            // its spacer cell so the cursor stays aligned with the model.
            ++x;
          }
        }
        if (y + 1 < size.h) {
          buf << "\r\n";
        }
      }

      ansi_wrap(buf, true);
      ansi_reset(buf);

      prev_.resize(size);
      prev_.blit(cur, glyph::core::Point{0, 0});
      has_prev_ = true;

      ansi_apply_cursor(buf, frame.cursor());
      prev_cursor_     = frame.cursor();
      has_prev_cursor_ = true;
      out_ << buf.str();
      out_.flush();
      return;
    }

    // Dirty lines only.
    const auto dirty_lines = frame.take_dirty_lines();
    if (dirty_lines.empty()) {
      reconcile_cursor(frame.cursor());
      return;
    }

    const auto                        prev_view = prev_.const_view();
    std::vector<glyph::core::coord_t> changed_lines;
    if (dirty_lines.size() < std::size_t(size.h / 4)) {
      changed_lines.reserve(dirty_lines.size());
      for (auto y : dirty_lines) {
        if (glyph::core::hash_line(prev_view, y) !=
            glyph::core::hash_line(cur, y)) {
          changed_lines.push_back(y);
        }
      }

      if (changed_lines.empty()) {
        reconcile_cursor(frame.cursor());
        return;
      }
    }
    else {
      changed_lines.assign(dirty_lines.begin(), dirty_lines.end());
    }

    ansi_wrap(buf, false);

    glyph::core::Style current{};
    bool               has_current = false;

    const auto spans = glyph::core::diff_spans(prev_view, cur, changed_lines);

    for (const auto &span : spans) {
      render_span(buf, cur, span, current, has_current);
    }

    ansi_wrap(buf, true);
    ansi_reset(buf);

    prev_.blit(cur, glyph::core::Point{0, 0});

    ansi_apply_cursor(buf, frame.cursor());
    prev_cursor_     = frame.cursor();
    has_prev_cursor_ = true;
    out_ << buf.str();
    out_.flush();
  }

  // Emit a cursor update directly to the stream, but only when the hint
  // differs from the last one applied — avoids redundant escape output on
  // frames where nothing (including the caret) moved.
  void AnsiRenderer::reconcile_cursor(const view::Frame::CursorHint &hint) {
    if (has_prev_cursor_ && prev_cursor_.visible == hint.visible &&
        prev_cursor_.pos == hint.pos) {
      return;
    }
    std::ostringstream buf;
    ansi_apply_cursor(buf, hint);
    out_ << buf.str();
    out_.flush();
    prev_cursor_     = hint;
    has_prev_cursor_ = true;
  }

} // namespace glyph::render

// glyph/render/terminal.h
//
// Terminal helpers for console sizing and session state.
//
// Responsibilities:
//   - Query current terminal size in character cells.
//   - Toggle alternate screen + cursor visibility with RAII.
//   - Provide a lightweight app wrapper for size + render.

#pragma once

#include "glyph/core/geometry.h"
#include "glyph/core/types.h"
#include "glyph/render/ansi/ansi_renderer.h"
#include <iosfwd>

namespace glyph::view {
  class Frame;
}

namespace glyph::render {

  // ------------------------------------------------------------
  // TerminalSize
  // ------------------------------------------------------------
  struct TerminalSize final {
    core::coord_t cols = 0;
    core::coord_t rows = 0;
    bool          valid = false;
  };

  // ------------------------------------------------------------
  // Query terminal size (cols/rows). Returns valid=false on failure.
  // Values are in terminal character cells, not pixels.
  // ------------------------------------------------------------
  TerminalSize get_terminal_size();

  // ------------------------------------------------------------
  // Compute a frame size using current terminal size or fallback.
  // ------------------------------------------------------------
  core::Size terminal_frame_size(core::Size fallback);

  // ------------------------------------------------------------
  // TerminalSessionOptions
  // ------------------------------------------------------------
  struct TerminalSessionOptions final {
    bool use_alt_screen = true;
    bool hide_cursor    = true;
  };

  // ------------------------------------------------------------
  // TerminalSession
  // ------------------------------------------------------------
  // RAII helper that toggles terminal state once on enter/exit.
  class TerminalSession final {
  public:
    explicit TerminalSession(std::ostream &out,
                             TerminalSessionOptions options = {});
    ~TerminalSession();

    TerminalSession(const TerminalSession &) = delete;
    TerminalSession &operator=(const TerminalSession &) = delete;

  private:
    std::ostream          &out_;
    TerminalSessionOptions options_{};
  };

  // ------------------------------------------------------------
  // TerminalApp
  // ------------------------------------------------------------
  // Convenience wrapper that owns a session + ANSI renderer.
  class TerminalApp final {
  public:
    explicit TerminalApp(std::ostream &out,
                         TerminalSessionOptions options = {});

    TerminalSize size() const;
    core::Size   frame_size(core::Size fallback = {80, 24}) const;

    void render(const view::Frame &frame);

  private:
    TerminalSession session_;
    AnsiRenderer    renderer_;
  };

} // namespace glyph::render

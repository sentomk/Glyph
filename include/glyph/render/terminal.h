// glyph/render/terminal.h
//
// Terminal helpers for console sizing and session state.
//
// Responsibilities:
//   - Query current terminal size in character cells.
//   - Toggle alternate screen + cursor visibility with RAII.

#pragma once

#include "glyph/core/types.h"
#include <iosfwd>

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

} // namespace glyph::render

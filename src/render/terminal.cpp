// glyph/render/terminal.cpp
//
// Terminal helpers with platform-specific implementations.

#include "glyph/render/terminal.h"

#include <ostream>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace glyph::render {

  TerminalSize get_terminal_size() {
    TerminalSize out{};

#if defined(_WIN32)
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE) {
      return out;
    }

    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(handle, &info)) {
      return out;
    }

    // Use the visible window (not the full buffer).
    const auto cols = info.srWindow.Right - info.srWindow.Left + 1;
    const auto rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    if (cols > 0 && rows > 0) {
      out.cols  = static_cast<core::coord_t>(cols);
      out.rows  = static_cast<core::coord_t>(rows);
      out.valid = true;
    }
#else
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
      if (ws.ws_col > 0 && ws.ws_row > 0) {
        out.cols  = static_cast<core::coord_t>(ws.ws_col);
        out.rows  = static_cast<core::coord_t>(ws.ws_row);
        out.valid = true;
      }
    }
#endif

    return out;
  }

  TerminalSession::TerminalSession(std::ostream &out,
                                   TerminalSessionOptions options)
      : out_(out), options_(options) {
    if (options_.use_alt_screen) {
      out_ << "\x1b[?1049h";
    }
    if (options_.hide_cursor) {
      out_ << "\x1b[?25l";
    }
  }

  TerminalSession::~TerminalSession() {
    if (options_.hide_cursor) {
      out_ << "\x1b[?25h";
    }
    if (options_.use_alt_screen) {
      out_ << "\x1b[?1049l";
    }
  }

} // namespace glyph::render

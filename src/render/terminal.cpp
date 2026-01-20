// glyph/render/terminal.cpp
//
// Terminal helpers with platform-specific implementations.

#if defined(GLYPH_USE_MODULES)
module;
#include <ostream>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif
module glyph;
#else
#include "glyph/render/terminal.h"
#include "glyph/view/frame.h"
#include <ostream>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif
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

  core::Size terminal_frame_size(core::Size fallback) {
    const auto term = get_terminal_size();
    const auto width = term.valid ? term.cols : fallback.w;
    const auto height = term.valid ? term.rows : fallback.h;
    return core::Size{width, height};
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

  TerminalApp::TerminalApp(std::ostream &out, TerminalSessionOptions options)
      : session_(out, options), renderer_(out) {
  }

  TerminalSize TerminalApp::size() const {
    return get_terminal_size();
  }

  core::Size TerminalApp::frame_size(core::Size fallback) const {
    return terminal_frame_size(fallback);
  }

  void TerminalApp::render(const view::Frame &frame) {
    renderer_.render(frame);
  }

  void TerminalApp::reset_renderer() {
    renderer_.reset();
  }

} // namespace glyph::render

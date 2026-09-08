// glyph/render/terminal.cpp
//
// Terminal helpers with platform-specific implementations.

#include "glyph/render/terminal.h"

#include <csignal>
#include <iostream>
#include <ostream>

#include "glyph/view/frame.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <termios.h>
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

  // ------------------------------------------------------------
  // FullScreenGuard
  // ------------------------------------------------------------

  namespace {

    // Terminal attributes saved on enter and restored on leave, plus the
    // active-guard bookkeeping the signal handlers need. File-scope so the
    // plain-C signal handler can reach them.
    struct SavedTerminal {
#if defined(_WIN32)
      DWORD in_mode = 0;
      DWORD out_mode = 0;
      bool have_modes = false;
#else
      termios tio{};
      bool   have_tio = false;
#endif
    } g_saved;

    FullScreenGuard *g_guard = nullptr;

#if !defined(_WIN32)
    void (*g_prev_sigint)(int)  = nullptr;
    void (*g_prev_sigterm)(int) = nullptr;
#endif

    // Restore terminal attributes and leave full-screen mode through
    // async-signal-safe primitives only (no iostreams): called both from
    // the destructor and from signal handlers.
    void restore_terminal_raw() noexcept {
#if defined(_WIN32)
      if (g_saved.have_modes) {
        if (HANDLE h = GetStdHandle(STD_INPUT_HANDLE)) {
          SetConsoleMode(h, g_saved.in_mode);
        }
        if (HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE)) {
          SetConsoleMode(h, g_saved.out_mode);
        }
      }
      const char seq[] = "\x1b[?25h\x1b[?7h\x1b[?1049l";
      DWORD written = 0;
      WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), seq,
                static_cast<DWORD>(sizeof seq - 1), &written, nullptr);
#else
      if (g_saved.have_tio) {
        ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_saved.tio);
      }
      const char seq[] = "\x1b[?25h\x1b[?7h\x1b[?1049l";
      ssize_t rc = ::write(STDOUT_FILENO, seq, sizeof seq - 1);
      (void)rc;
#endif
    }

#if defined(_WIN32)
    BOOL WINAPI guard_ctrl_handler(DWORD) {
      restore_terminal_raw();
      return FALSE; // continue with default termination
    }
#else
    void guard_signal_handler(int sig) {
      restore_terminal_raw();
      ::signal(sig, SIG_DFL);
      ::raise(sig);
    }
#endif

  } // namespace

  FullScreenGuard::FullScreenGuard() : out_(std::cout) {
    enter();
  }

  FullScreenGuard::FullScreenGuard(std::ostream &out) : out_(out) {
    enter();
  }

  void FullScreenGuard::enter() {
    // Save terminal attributes before the first mode change.
#if defined(_WIN32)
    if (HANDLE hin = GetStdHandle(STD_INPUT_HANDLE)) {
      g_saved.have_modes =
          GetConsoleMode(hin, &g_saved.in_mode) != 0;
    }
    if (HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE)) {
      DWORD mode = 0;
      if (GetConsoleMode(hout, &mode)) {
        g_saved.out_mode  = mode;
        g_saved.have_modes = true;
        // VT sequences only work when enabled; turn them on for the
        // guard's lifetime (restored from out_mode on leave).
        SetConsoleMode(hout, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
      }
    }
#else
    if (::isatty(STDIN_FILENO)) {
      g_saved.have_tio = ::tcgetattr(STDIN_FILENO, &g_saved.tio) == 0;
    }
#endif

    out_ << "\x1b[?1049h"   // alternate screen buffer
          << "\x1b[?25l"    // hide cursor
          << "\x1b[?7l"     // disable autowrap
          << std::flush;

#if defined(_WIN32)
    SetConsoleCtrlHandler(&guard_ctrl_handler, TRUE);
#else
    g_prev_sigint  = ::signal(SIGINT, &guard_signal_handler);
    g_prev_sigterm = ::signal(SIGTERM, &guard_signal_handler);
#endif
    g_guard = this;
  }

  void FullScreenGuard::leave() noexcept {
    g_guard = nullptr;

#if defined(_WIN32)
    SetConsoleCtrlHandler(&guard_ctrl_handler, FALSE);
#else
    if (g_prev_sigint != nullptr) {
      ::signal(SIGINT, g_prev_sigint);
      g_prev_sigint = nullptr;
    }
    if (g_prev_sigterm != nullptr) {
      ::signal(SIGTERM, g_prev_sigterm);
      g_prev_sigterm = nullptr;
    }
#endif

    try {
      out_ << "\x1b[?25h"   // show cursor
           << "\x1b[?7h"    // re-enable autowrap
           << "\x1b[?1049l" // back to the primary screen buffer
           << std::flush;
    } catch (...) {
      // Restoration must not throw from a destructor.
    }

#if defined(_WIN32)
    if (g_saved.have_modes) {
      if (HANDLE h = GetStdHandle(STD_INPUT_HANDLE)) {
        SetConsoleMode(h, g_saved.in_mode);
      }
      if (HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE)) {
        SetConsoleMode(h, g_saved.out_mode);
      }
      g_saved.have_modes = false;
    }
#else
    if (g_saved.have_tio) {
      ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_saved.tio);
      g_saved.have_tio = false;
    }
#endif
  }

  FullScreenGuard::~FullScreenGuard() {
    if (g_guard == this) {
      leave();
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

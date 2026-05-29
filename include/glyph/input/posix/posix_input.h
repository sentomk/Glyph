// glyph/input/posix/posix_input.h
//
// POSIX terminal input backend (macOS / Linux / BSD).
//
// Responsibilities:
//   - Put the controlling terminal into raw mode via termios.
//   - Read stdin bytes, UTF-8 decode, and feed the shared VT decoder.
//   - Toggle SGR mouse reporting and bracketed paste on stdout.
//   - Surface terminal resize (SIGWINCH) as ResizeEvent.
//
// Scope: terminal emulators (Ghostty / iTerm2 / GNOME Terminal / xterm).
// Out of scope: Linux bare framebuffer TTY.

#pragma once

#include <cstdint>
#include <deque>

#include <termios.h>

#include "glyph/core/event.h"
#include "glyph/input/detail/vt_decoder.h"
#include "glyph/input/input.h"

namespace glyph::input {

  class PosixInput final : public Input {
  public:
    PosixInput();
    ~PosixInput() override;

    PosixInput(const PosixInput &)            = delete;
    PosixInput &operator=(const PosixInput &) = delete;

    core::Event poll() override;
    core::Event read() override;

    void      set_mode(InputMode mode) override;
    InputMode get_mode() const override;

  private:
    // Read available bytes (non-blocking), decode, and queue events.
    void pump(bool block);
    // Move any decoder output into pending_.
    void drain_decoder();
    // Decode raw bytes (UTF-8) into the VT decoder, holding partial
    // multi-byte sequences across calls.
    void feed_bytes(const char *data, std::size_t len);
    // Emit a ResizeEvent if SIGWINCH fired since last check.
    bool poll_resize(core::Event &out);

    void write_seq(const char *seq) const;
    void apply_mouse(bool enable);
    void apply_paste(bool enable);

    detail::VtDecoder       decoder_{};
    std::deque<core::Event> pending_{};

    int            fd_in_  = -1; // STDIN_FILENO
    int            fd_out_ = -1; // STDOUT_FILENO
    bool           tty_         = false;
    bool           raw_active_  = false;
    bool           mouse_active_ = false;
    bool           paste_active_ = false;
    InputMode      mode_        = InputMode::None;
    struct termios orig_termios_ {};

    // UTF-8 incremental decode state.
    char32_t      cp_       = 0;
    std::uint8_t  cp_need_  = 0; // continuation bytes still expected
  };

} // namespace glyph::input

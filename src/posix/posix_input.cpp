// glyph/input/posix/posix_input.cpp
//
// POSIX terminal input backend implementation.

#include "glyph/input/posix/posix_input.h"

#include <atomic>
#include <csignal>
#include <cstring>

#include <poll.h>
#include <unistd.h>

namespace glyph::input {

  namespace {
    // Set by the SIGWINCH handler; consumed by poll_resize().
    std::atomic<bool> g_winch_flag{false};

    void handle_sigwinch(int) {
      g_winch_flag.store(true, std::memory_order_relaxed);
    }
  } // namespace

  PosixInput::PosixInput() {
    fd_in_  = STDIN_FILENO;
    fd_out_ = STDOUT_FILENO;
    tty_    = ::isatty(fd_in_) != 0;

    if (tty_) {
      ::tcgetattr(fd_in_, &orig_termios_);
    }

    // Install a resize handler. Existing handler is replaced for the
    // lifetime of the process; restored to default on destruction.
    struct sigaction sa {};
    sa.sa_handler = &handle_sigwinch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    ::sigaction(SIGWINCH, &sa, nullptr);
  }

  PosixInput::~PosixInput() {
    if (paste_active_) {
      apply_paste(false);
    }
    if (mouse_active_) {
      apply_mouse(false);
    }
    if (tty_ && raw_active_) {
      ::tcsetattr(fd_in_, TCSAFLUSH, &orig_termios_);
    }

    struct sigaction sa {};
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    ::sigaction(SIGWINCH, &sa, nullptr);
  }

  InputMode PosixInput::get_mode() const {
    return mode_;
  }

  void PosixInput::write_seq(const char *seq) const {
    if (fd_out_ < 0 || seq == nullptr) {
      return;
    }
    const std::size_t len = std::strlen(seq);
    std::size_t       off = 0;
    while (off < len) {
      const ssize_t n = ::write(fd_out_, seq + off, len - off);
      if (n <= 0) {
        break;
      }
      off += static_cast<std::size_t>(n);
    }
  }

  void PosixInput::apply_mouse(bool enable) {
    // 1000 = button events, 1006 = SGR extended coordinates.
    write_seq(enable ? "\x1b[?1000h\x1b[?1006h" : "\x1b[?1000l\x1b[?1006l");
    mouse_active_ = enable;
  }

  void PosixInput::apply_paste(bool enable) {
    write_seq(enable ? "\x1b[?2004h" : "\x1b[?2004l");
    paste_active_ = enable;
  }

  void PosixInput::set_mode(InputMode mode) {
    const bool want_raw   = (mode & InputMode::Raw) != InputMode::None;
    const bool want_mouse = (mode & InputMode::Mouse) != InputMode::None;
    const bool want_paste = (mode & InputMode::Paste) != InputMode::None;

    if (tty_) {
      if (want_raw && !raw_active_) {
        struct termios raw = orig_termios_;
        ::cfmakeraw(&raw);
        // Non-blocking semantics; blocking handled by poll() at the
        // syscall level, not by VMIN/VTIME.
        raw.c_cc[VMIN]  = 0;
        raw.c_cc[VTIME] = 0;
        ::tcsetattr(fd_in_, TCSAFLUSH, &raw);
        raw_active_ = true;
      }
      else if (!want_raw && raw_active_) {
        ::tcsetattr(fd_in_, TCSAFLUSH, &orig_termios_);
        raw_active_ = false;
      }
    }

    if (want_mouse != mouse_active_) {
      apply_mouse(want_mouse);
    }
    if (want_paste != paste_active_) {
      apply_paste(want_paste);
    }

    mode_ = mode;
  }

  void PosixInput::drain_decoder() {
    while (decoder_.has_event()) {
      pending_.push_back(decoder_.pop());
    }
  }

  void PosixInput::feed_bytes(const char *data, std::size_t len) {
    for (std::size_t i = 0; i < len; ++i) {
      const auto byte = static_cast<unsigned char>(data[i]);

      if (cp_need_ == 0) {
        // Lead byte.
        if (byte < 0x80) {
          decoder_.feed(static_cast<char32_t>(byte));
        }
        else if ((byte & 0xE0) == 0xC0) {
          cp_      = byte & 0x1F;
          cp_need_ = 1;
        }
        else if ((byte & 0xF0) == 0xE0) {
          cp_      = byte & 0x0F;
          cp_need_ = 2;
        }
        else if ((byte & 0xF8) == 0xF0) {
          cp_      = byte & 0x07;
          cp_need_ = 3;
        }
        else {
          // Invalid lead byte: emit replacement, stay in sync.
          decoder_.feed(U'�');
        }
      }
      else {
        // Continuation byte.
        if ((byte & 0xC0) == 0x80) {
          cp_ = (cp_ << 6) | (byte & 0x3F);
          if (--cp_need_ == 0) {
            decoder_.feed(cp_);
            cp_ = 0;
          }
        }
        else {
          // Malformed sequence: drop partial, reprocess this byte.
          cp_      = 0;
          cp_need_ = 0;
          --i; // re-evaluate as a fresh lead byte
        }
      }
    }
  }

  bool PosixInput::poll_resize(core::Event &out) {
    if (!g_winch_flag.exchange(false, std::memory_order_relaxed)) {
      return false;
    }
    core::ResizeEvent ev{};
    // Size is reported lazily; callers typically re-query terminal size.
    // We still surface the event so apps can trigger a relayout.
    out = ev;
    return true;
  }

  void PosixInput::pump(bool block) {
    if (fd_in_ < 0) {
      return;
    }

    struct pollfd pfd {};
    pfd.fd     = fd_in_;
    pfd.events = POLLIN;

    const int timeout = block ? -1 : 0;
    const int rc      = ::poll(&pfd, 1, timeout);
    if (rc <= 0) {
      // Timeout or interrupted (e.g. by SIGWINCH): nothing to read.
      if (!block) {
        decoder_.flush(true);
        drain_decoder();
      }
      return;
    }

    if (pfd.revents & POLLIN) {
      char          buf[512];
      const ssize_t n = ::read(fd_in_, buf, sizeof(buf));
      if (n > 0) {
        feed_bytes(buf, static_cast<std::size_t>(n));
        drain_decoder();
      }
    }

    // If the read drained the buffer, resolve any lone ESC.
    if (!block) {
      struct pollfd again {};
      again.fd     = fd_in_;
      again.events = POLLIN;
      if (::poll(&again, 1, 0) == 0) {
        decoder_.flush(true);
        drain_decoder();
      }
    }
  }

  core::Event PosixInput::poll() {
    if (!pending_.empty()) {
      auto ev = pending_.front();
      pending_.pop_front();
      return ev;
    }

    core::Event resize{};
    if (poll_resize(resize)) {
      return resize;
    }

    pump(false);

    if (!pending_.empty()) {
      auto ev = pending_.front();
      pending_.pop_front();
      return ev;
    }
    return std::monostate{};
  }

  core::Event PosixInput::read() {
    for (;;) {
      if (!pending_.empty()) {
        auto ev = pending_.front();
        pending_.pop_front();
        return ev;
      }

      core::Event resize{};
      if (poll_resize(resize)) {
        return resize;
      }

      pump(true);

      if (!pending_.empty()) {
        auto ev = pending_.front();
        pending_.pop_front();
        return ev;
      }
      // poll() returned due to SIGWINCH with no bytes: loop to surface
      // the resize event on the next iteration.
    }
  }

} // namespace glyph::input

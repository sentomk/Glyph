// e2e: SIGWINCH must yield a ResizeEvent carrying the new terminal size.
//
// The parent resizes the pty (TIOCSWINSZ), which makes the kernel send
// SIGWINCH to the foreground group; the child runs PosixInput, waits for
// the ResizeEvent, and reports the size it received.

#if !defined(_WIN32)

#include <doctest/doctest.h>

#include <cstdio>
#include <csignal>
#include <string>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <poll.h>

#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif

#include "glyph/core/event.h"
#include "glyph/input/posix/posix_input.h"

TEST_CASE("resize events carry the new terminal size") {
  int master = -1;
  int slave  = -1;
  REQUIRE(::openpty(&master, &slave, nullptr, nullptr, nullptr) == 0);

  // Initial size, so the later change is observable.
  winsize ws{};
  ws.ws_col = 10;
  ws.ws_row = 5;
  REQUIRE(::ioctl(master, TIOCSWINSZ, &ws) == 0);

  const pid_t pid = ::fork();
  REQUIRE(pid >= 0);

  if (pid == 0) {
    if (::setsid() < 0 ||
        ::ioctl(slave, TIOCSCTTY, 0) != 0 ||
        ::dup2(slave, STDIN_FILENO) < 0 ||
        ::dup2(slave, STDOUT_FILENO) < 0) {
      ::_exit(72);
    }
    ::close(master);
    ::close(slave);

    glyph::input::PosixInput in;
    // The SIGWINCH handler is installed by the constructor; only report
    // ready afterwards, or the parent's resize races us and the signal
    // is dropped (default SIGWINCH disposition is ignore).
    ::write(STDOUT_FILENO, "RDY\n", 4);

    for (int i = 0; i < 300; ++i) { // ~3s deadline
      const auto ev = in.poll();
      if (const auto *r =
              std::get_if<glyph::core::ResizeEvent>(&ev)) {
        char buf[64];
        const int n = std::snprintf(buf, sizeof buf, "RES %d %d\n",
                                    r->size.w, r->size.h);
        ::write(STDOUT_FILENO, buf, static_cast<size_t>(n));
        ::_exit(0);
      }
      ::usleep(10000);
    }
    ::_exit(80); // no resize observed within the deadline
  }

  // Drop our slave copy: while it is open, a Linux master read never
  // sees EOF/EIO once the child is gone (see test_raw_restore).
  ::close(slave);

  // Wait for the child's ready marker (bounded), then resize the pty,
  // which raises SIGWINCH in the child's foreground group.
  std::string got;
  {
    bool ready = false;
    while (!ready) {
      struct pollfd pfd {};
      pfd.fd     = master;
      pfd.events = POLLIN;
      if (::poll(&pfd, 1, 5000) <= 0 ||
          !(pfd.revents & (POLLIN | POLLHUP | POLLERR))) {
        break;
      }
      char          buf[256];
      const ssize_t n = ::read(master, buf, sizeof buf);
      if (n <= 0) {
        break;
      }
      got.append(buf, static_cast<std::size_t>(n));
      ready = got.find("RDY") != std::string::npos;
    }
    REQUIRE(got.find("RDY") != std::string::npos);
  }

  winsize ws2{};
  ws2.ws_col = 24;
  ws2.ws_row = 12;
  REQUIRE(::ioctl(master, TIOCSWINSZ, &ws2) == 0);

  int status = 0;
  for (;;) {
    struct pollfd pfd {};
    pfd.fd     = master;
    pfd.events = POLLIN;
    if (::poll(&pfd, 1, 5000) <= 0 ||
        !(pfd.revents & (POLLIN | POLLHUP | POLLERR))) {
      REQUIRE(::waitpid(pid, &status, 0) == pid);
      break;
    }
    char          buf[256];
    const ssize_t n = ::read(master, buf, sizeof buf);
    if (n > 0) {
      got.append(buf, static_cast<std::size_t>(n));
    }
    if (::waitpid(pid, &status, WNOHANG) == pid) {
      break;
    }
    if (n < 0) {
      REQUIRE(::waitpid(pid, &status, 0) == pid);
      break;
    }
  }

  REQUIRE(WIFEXITED(status));
  CHECK(WEXITSTATUS(status) == 0);
  CHECK(got.find("RES 24 12") != std::string::npos);

  ::close(master);
}

#endif // !_WIN32

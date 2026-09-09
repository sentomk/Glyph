// e2e: raw-mode terminal state must survive a fatal signal.
//
// The child enters raw mode on a fresh pty and kills itself with
// SIGTERM; the parent asserts the child died by the signal (handler
// re-raised) and that the pty came back cooked (ECHO/ICANON/OPOST).

#if !defined(_WIN32)

#include <doctest/doctest.h>

#include <csignal>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif

#include "glyph/input/posix/posix_input.h"

TEST_CASE("raw mode is restored when the process dies by signal") {
  int master = -1;
  int slave  = -1;
  REQUIRE(::openpty(&master, &slave, nullptr, nullptr, nullptr) == 0);

  termios cooked{};
  REQUIRE(::tcgetattr(slave, &cooked) == 0);
  REQUIRE((cooked.c_lflag & ECHO) != 0);

  const pid_t pid = ::fork();
  REQUIRE(pid >= 0);

  if (pid == 0) {
    // Child: detach and make the pty the controlling terminal + stdio.
    // setsid() returns the new session id (positive) on success.
    if (::setsid() < 0 ||
        ::ioctl(slave, TIOCSCTTY, 0) != 0 ||
        ::dup2(slave, STDIN_FILENO) < 0 ||
        ::dup2(slave, STDOUT_FILENO) < 0) {
      ::_exit(72);
    }
    ::close(master);
    ::close(slave);

    glyph::input::PosixInput in;
    in.set_mode(glyph::input::InputMode::Raw);
    // Out-of-band death: no destructor runs if the handler is missing.
    ::kill(::getpid(), SIGTERM);
    ::_exit(0); // unreachable when the handler re-raises
  }

  // Service the master side like a real terminal would while reaping:
  // the dying child's restore sequences must be drained or its exit
  // wedges on the tty output queue. read() returns EIO once the child
  // side has fully closed, which ends the loop.
  int status = 0;
  for (;;) {
    char buf[256];
    const ssize_t n = ::read(master, buf, sizeof buf);
    if (::waitpid(pid, &status, WNOHANG) == pid) {
      break;
    }
    if (n < 0) {
      REQUIRE(::waitpid(pid, &status, 0) == pid);
      break;
    }
  }
  CHECK(WIFSIGNALED(status));
  CHECK(WTERMSIG(status) == SIGTERM);

  termios after{};
  REQUIRE(::tcgetattr(master, &after) == 0);
  CHECK((after.c_lflag & ECHO) != 0);
  CHECK((after.c_lflag & ICANON) != 0);
  CHECK((after.c_oflag & OPOST) != 0);

  ::close(master);
  ::close(slave);
}

#endif // !_WIN32

// glyph/platform/clipboard.cpp
//
// Clipboard via the platform's standard tool. All backends are pipe-based
// (popen), so no new link dependencies are introduced.

#include "glyph/platform/clipboard.h"

#include <cstdio>
#include <string>

#if defined(_WIN32)
#define GLYPH_POPEN  _popen
#define GLYPH_PCLOSE _pclose
#else
#define GLYPH_POPEN  popen
#define GLYPH_PCLOSE pclose
#endif

namespace glyph::platform {

  namespace {

    // Run `command` with text on its stdin; true when the tool ran and
    // accepted all bytes. popen itself succeeds even for a missing
    // command (the shell reports 127 at pclose), so the exit status is
    // the availability signal.
    bool pipe_to(const char *command, std::string_view text) {
      std::FILE *pipe = GLYPH_POPEN(command, "w");
      if (pipe == nullptr) {
        return false;
      }
      const std::size_t written =
          std::fwrite(text.data(), 1, text.size(), pipe);
      const int        status = GLYPH_PCLOSE(pipe);
      return written == text.size() && status == 0;
    }

  } // namespace

  bool copy_to_clipboard(std::string_view text) {
    if (text.empty()) {
      return false;
    }

#if defined(__APPLE__)
    return pipe_to("pbcopy", text);
#elif defined(_WIN32)
    return pipe_to("clip.exe", text);
#else
    // Wayland first, X11 fallback; headless boxes have neither and
    // report failure through the 127 exit status.
    if (pipe_to("wl-copy", text)) {
      return true;
    }
    return pipe_to("xclip -selection clipboard -in", text);
#endif
  }

} // namespace glyph::platform

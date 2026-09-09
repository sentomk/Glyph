// glyph/platform/clipboard.h
//
// Platform clipboard access.

#pragma once

#include <string_view>

namespace glyph::platform {

  // Copy UTF-8 text to the system clipboard.
  //
  // Backend per platform: pbcopy (macOS), wl-copy / xclip (Linux, tried
  // in that order), clip.exe (Windows). Returns false when no backend
  // is installed or the write fails.
  bool copy_to_clipboard(std::string_view text);

} // namespace glyph::platform

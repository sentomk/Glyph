// glyph/input/make_input.cpp
//
// Platform dispatch for the default input backend.

#include "glyph/input/input.h"

#if defined(_WIN32)
#include "glyph/input/win32/win_input.h"
#else
#include "glyph/input/posix/posix_input.h"
#endif

namespace glyph::input {

  std::unique_ptr<Input> make_default_input() {
#if defined(_WIN32)
    return std::make_unique<WinInput>();
#else
    return std::make_unique<PosixInput>();
#endif
  }

} // namespace glyph::input

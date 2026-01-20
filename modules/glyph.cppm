// modules/glyph.cppm
//
// glyph module interface.
//
// Responsibilities:
//   - Export glyph as a single umbrella module.

module;
#include "glyph.prelude.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

export module glyph;

export {
  // core/
  #include "glyph/core/buffer.h"
  #include "glyph/core/cell.h"
  #include "glyph/core/color.h"
  #include "glyph/core/diff.h"
  #include "glyph/core/event.h"
  #include "glyph/core/geometry.h"
  #include "glyph/core/style.h"
  #include "glyph/core/text.h"
  #include "glyph/core/types.h"

  // input/
  #include "glyph/input/input.h"
  #include "glyph/input/input_guard.h"
  #include "glyph/input/win32/win_input.h"

  // render/
  #include "glyph/render/render.h"
  #include "glyph/render/terminal.h"
  #include "glyph/render/ansi/ansi_renderer.h"
  #include "glyph/render/debug/debug_renderer.h"

  // view/
  #include "glyph/view/canvas.h"
  #include "glyph/view/frame.h"
  #include "glyph/view/text.h"
  #include "glyph/view/view.h"
  // view/components
  #include "glyph/view/components/border.h"
  #include "glyph/view/components/bar.h"
  #include "glyph/view/components/fill.h"
  #include "glyph/view/components/focus.h"
  #include "glyph/view/components/inset.h"
  #include "glyph/view/components/label.h"
  #include "glyph/view/components/panel.h"
  #include "glyph/view/components/stack.h"
  #include "glyph/view/components/table.h"
  // view/layout
  #include "glyph/view/layout/align.h"
  #include "glyph/view/layout/box.h"
  #include "glyph/view/layout/inset.h"
  #include "glyph/view/layout/scroll.h"
  #include "glyph/view/layout/split.h"
  #include "glyph/view/layout/types.h"
}

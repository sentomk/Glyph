// glyph/render/ansi/ansi_renderer.h
//
// ANSI renderer (VT escape sequences).
//
// Responsibilities:
//   - Consume a view::Frame and emit ANSI sequences + glyphs.
//   - Full redraw on first frame or size change.
//   - Diff-based updates on dirty lines between frames.

#pragma once

#include "glyph/core/buffer.h"
#include "glyph/render/render.h"
#include <iosfwd>
namespace glyph::render {

  class AnsiRenderer final : public Renderer {
  public:
    explicit AnsiRenderer(std::ostream &out) noexcept;

    void render(const view::Frame &frame) override;

  private:
    std::ostream       &out_;
    glyph::core::Buffer prev_{};
    bool                has_prev_ = false;
  };

} // namespace glyph::render

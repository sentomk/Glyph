// glyph/render/ansi/ansi_renderer.h
//
// ANSI renderer (VT escape sequences).
//
// Responsibilities:
//   - Consume a view::Frame and emit ANSI sequences + glyphs.
//   - Keep output deterministic and stateless for now (full-frame paint).

#pragma once

#include "glyph/render/render.h"
#include <iosfwd>
namespace glyph::render {

  class AnsiRenderer final : public Renderer {
  public:
    explicit AnsiRenderer(std::ostream &out) noexcept;

    void render(const view::Frame &frame) override;

  private:
    std::ostream &out_;
  };

} // namespace glyph::render
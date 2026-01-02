// glyph/render/debug/debug_renderer.h
//
// Debug renderer used to validate the render pipeline.
// This renderer performs structured traversal over view::Frame
// and emits a textual representation of each render stage.

#pragma once

#include "glyph/render/render.h"
#include <iosfwd>

namespace glyph::render {

  class DebugRenderer final : public Renderer {

  public:
    explicit DebugRenderer(std::ostream &out) noexcept;

    void render(const view::Frame &frame) override;

  private:
    std::ostream &out_;
  };
} // namespace glyph::render
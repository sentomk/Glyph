// glyph/render/render.h
//
// Frame consumer interface.
//
// Responsibilities:
//   - Define the terminal stage of the pipeline (consume a completed Frame)
//   - Keep render side effects out of view/ and core/
//   - Avoid coupling to any specific backend (ANSI, Win32, ncurses, ...)

#pragma once

namespace glyph::view {
  class Frame; // forward declare.
}

namespace glyph::render {

  // ------------------------------------------------------------
  // Renderer: a sink that consumes a fully built view::Frame.
  // ------------------------------------------------------------
  class Renderer {
  public:
    virtual ~Renderer() = default;

    Renderer()                            = default;
    Renderer(const Renderer &)            = delete;
    Renderer &operator=(const Renderer &) = delete;

    virtual void render(const view::Frame &frame) = 0;
  };

} // namespace glyph::render
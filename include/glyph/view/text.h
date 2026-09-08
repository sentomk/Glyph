// glyph/view/text.h
//
// Text drawing helpers for Frame.
//
// Responsibilities:
//   - Draw ASCII/UTF-32 strings into a Frame with clipping.

#pragma once

#include <string_view>

#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/core/text.h"
#include "glyph/view/canvas.h"
#include "glyph/view/frame.h"

namespace glyph::view {

  // ------------------------------------------------------------
  // Display width of UTF-8 text in columns (grapheme-aware).
  // ------------------------------------------------------------
  inline core::coord_t text_width(std::string_view text) noexcept {
    core::coord_t w     = 0;
    std::size_t   pos   = 0;
    while (pos < text.size()) {
      const core::Grapheme g = core::next_grapheme(text, pos);
      pos = g.next;
      w   = core::coord_t(w + g.width);
    }
    return w;
  }

  // ------------------------------------------------------------
  // Longest prefix of UTF-8 text that fits within max_columns.
  // Wide glyphs are never split; trailing zero-width clusters are
  // dropped. Returns the whole string when it already fits.
  // ------------------------------------------------------------
  inline std::string_view clip_to_width(std::string_view text,
                                        core::coord_t    max_columns) noexcept {
    if (max_columns <= 0) {
      return std::string_view{};
    }
    core::coord_t w     = 0;
    std::size_t   pos   = 0;
    std::size_t   limit = 0;
    while (pos < text.size()) {
      const core::Grapheme g = core::next_grapheme(text, pos);
      if (g.width > 0) {
        if (w + g.width > max_columns) {
          break;
        }
        w = core::coord_t(w + g.width);
      }
      pos   = g.next;
      limit = g.next;
    }
    return text.substr(0, limit);
  }

  // ------------------------------------------------------------
  // Draw UTF-8 text starting at p. Stops at frame width.
  // Grapheme-cluster aware: zero-width members (ZWJ, variation selectors,
  // combining marks, skin tones) never occupy a cell; multi-codepoint
  // clusters are stored as their base glyph.
  // ------------------------------------------------------------
  inline void draw_text(Frame &f, core::Point p, std::string_view text,
                        core::Cell cell = core::Cell::from_char(U' ')) {
    core::coord_t x   = p.x;
    std::size_t   pos = 0;
    while (pos < text.size()) {
      if (x >= f.size().w) {
        break;
      }
      const core::Grapheme g = core::next_grapheme(text, pos);
      pos = g.next;
      if (g.width == 0) {
        continue;
      }
      if (x + g.width > f.size().w) {
        break;
      }
      core::Cell c = cell;
      c.ch         = g.base;
      c.width      = g.width;
      f.set(core::Point{x, p.y}, c);
      x = core::coord_t(x + g.width);
    }
  }

  // ------------------------------------------------------------
  // Draw UTF-32 text starting at p. Stops at frame width.
  // ------------------------------------------------------------
  inline void draw_text(Frame &f, core::Point p, std::u32string_view text,
                        core::Cell cell = core::Cell::from_char(U' ')) {
    core::coord_t x = p.x;
    for (char32_t ch : text) {
      if (x >= f.size().w) {
        break;
      }
      const core::coord_t w = core::coord_t(core::cell_width(ch));
      if (w <= 0) {
        continue;
      }
      if (x + w > f.size().w) {
        break;
      }
      core::Cell c = cell;
      c.ch         = ch;
      c.width      = static_cast<std::uint8_t>(w);
      f.set(core::Point{x, p.y}, c);
      x = core::coord_t(x + w);
    }
  }

  // ------------------------------------------------------------
  // Draw UTF-8 text into a Canvas starting at p. Cluster-aware, same
  // semantics as the Frame overload above.
  // ------------------------------------------------------------
  inline void draw_text(Canvas &c, core::Point p, std::string_view text,
                        core::Cell cell = core::Cell::from_char(U' ')) {
    core::coord_t x   = p.x;
    std::size_t   pos = 0;
    while (pos < text.size()) {
      if (x >= c.size().w) {
        break;
      }
      const core::Grapheme g = core::next_grapheme(text, pos);
      pos = g.next;
      if (g.width == 0) {
        continue;
      }
      if (x + g.width > c.size().w) {
        break;
      }
      core::Cell out = cell;
      out.ch         = g.base;
      out.width      = g.width;
      c.set(core::Point{x, p.y}, out);
      x = core::coord_t(x + g.width);
    }
  }

  // ------------------------------------------------------------
  // Draw UTF-32 text into a Canvas starting at p.
  // ------------------------------------------------------------
  inline void draw_text(Canvas &c, core::Point p, std::u32string_view text,
                        core::Cell cell = core::Cell::from_char(U' ')) {
    core::coord_t x = p.x;
    for (char32_t ch : text) {
      if (x >= c.size().w) {
        break;
      }
      const core::coord_t w = core::coord_t(core::cell_width(ch));
      if (w <= 0) {
        continue;
      }
      if (x + w > c.size().w) {
        break;
      }
      core::Cell out = cell;
      out.ch         = ch;
      out.width      = static_cast<std::uint8_t>(w);
      c.set(core::Point{x, p.y}, out);
      x = core::coord_t(x + w);
    }
  }

} // namespace glyph::view

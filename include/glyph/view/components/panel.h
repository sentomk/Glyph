// glyph/view/components/panel.h
//
// PanelView: decorate a child view with optional fill, border, and padding.
//
// Responsibilities:
//   - Fill the entire area with a background cell (optional).
//   - Draw a single-cell border around the perimeter (optional).
//   - Render a child View inside the padded inner area.

#pragma once

#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/core/types.h"
#include "glyph/view/layout/inset.h"
#include "glyph/view/view.h"
#include "glyph/view/frame.h"

namespace glyph::view {

  // ------------------------------------------------------------
  // PanelStyle
  // ------------------------------------------------------------
  struct PanelStyle final {
    core::Cell     fill_cell{core::Cell::from_char(U' ')};
    core::Cell     border_cell{core::Cell::from_char(U'#')};
    layout::Insets padding{};
    bool           draw_fill   = false;
    bool           draw_border = false;

    static PanelStyle card(core::Color border_color,
                           char32_t   border_char = U'+',
                           layout::Insets padding = layout::Insets::all(1)) {
      PanelStyle style{};
      style.fill_cell = core::Cell::from_char(U' ');
      style.border_cell =
          core::Cell(border_char, core::Style{}.fg(border_color));
      style.padding     = padding;
      style.draw_fill   = true;
      style.draw_border = true;
      return style;
    }

    PanelStyle with_border(core::Color border_color,
                           char32_t   border_char) const {
      PanelStyle style = *this;
      style.border_cell =
          core::Cell(border_char, core::Style{}.fg(border_color));
      style.draw_border = true;
      return style;
    }

    PanelStyle with_border_color(core::Color border_color) const {
      return with_border(border_color, border_cell.ch);
    }

    PanelStyle with_padding(layout::Insets insets) const {
      PanelStyle style = *this;
      style.padding    = insets;
      return style;
    }

    PanelStyle with_fill(core::Cell cell) const {
      PanelStyle style = *this;
      style.fill_cell  = cell;
      style.draw_fill  = true;
      return style;
    }
  };

  // ------------------------------------------------------------
  // PanelView
  // ------------------------------------------------------------
  class PanelView : public View {

  public:
    static PanelView card(const View *child, core::Color border_color,
                          char32_t border_char = U'+',
                          layout::Insets padding =
                              layout::Insets::all(1)) {
      PanelView panel(child);
      panel.set_style(PanelStyle::card(border_color, border_char, padding));
      return panel;
    }

    static PanelView header(const View *child, core::Color border_color,
                            char32_t border_char = U'=',
                            layout::Insets padding =
                                layout::Insets::hv(2, 1)) {
      return card(child, border_color, border_char, padding);
    }

    // Construct a panel with an optional child.
    explicit PanelView(const View *child = nullptr) : child_(child) {
    }

    // Set or replace the child view (non-owning).
    void set_child(const View *child) {
      child_ = child;
    }

    // Enable background fill and set the fill cell.
    void set_fill(core::Cell cell) {
      fill_cell_ = cell;
      draw_fill_ = true;
    }

    // Enable border and set the border cell.
    void set_border(core::Cell cell) {
      border_cell_ = cell;
      draw_border_ = true;
    }

    // Set padding (inset) applied to the child area.
    void set_padding(layout::Insets insets) {
      padding_ = insets;
    }

    // Apply a reusable style preset.
    void set_style(const PanelStyle &style) {
      fill_cell_   = style.fill_cell;
      border_cell_ = style.border_cell;
      padding_     = style.padding;
      draw_fill_   = style.draw_fill;
      draw_border_ = style.draw_border;
    }

    // Toggle fill rendering without changing the cell value.
    void set_draw_fill(bool enabled) {
      draw_fill_ = enabled;
    }

    // Toggle border rendering without changing the cell value.
    void set_draw_border(bool enabled) {
      draw_border_ = enabled;
    }

    // Render sequence:
    //   1) optional fill (entire area)
    //   2) optional border (perimeter only)
    //   3) child render within padded inner rect
    void render(Frame &f, core::Rect area) const override {
      if (area.empty()) {
        return;
      }

      if (draw_fill_) {
        f.fill_rect(area, fill_cell_);
      }

      if (draw_border_) {
        draw_border(f, area);
      }

      if (child_ == nullptr) {
        return;
      }

      const auto inner = layout::inset_rect(area, clamp_padding(padding_));
      child_->render(f, inner);
    }

  private:
    // Ensure padding is non-negative to avoid inverted rects.
    static layout::Insets clamp_padding(layout::Insets insets) {
      if (insets.left < 0)
        insets.left = 0;
      if (insets.top < 0)
        insets.top = 0;
      if (insets.right < 0)
        insets.right = 0;
      if (insets.bottom < 0)
        insets.bottom = 0;
      return insets;
    }

    // Draw a simple single-cell border on all sides.
    void draw_border(Frame &f, core::Rect area) const {
      const core::coord_t x0 = area.left();
      const core::coord_t y0 = area.top();
      const core::coord_t x1 = area.right() - 1;
      const core::coord_t y1 = area.bottom() - 1;

      if (x0 > x1 || y0 > y1) {
        return;
      }

      for (core::coord_t x = x0; x <= x1; ++x) {
        f.set(core::Point{x, y0}, border_cell_);
        f.set(core::Point{x, y1}, border_cell_);
      }

      for (core::coord_t y = y0; y <= y1; ++y) {
        f.set(core::Point{x0, y}, border_cell_);
        f.set(core::Point{x1, y}, border_cell_);
      }
    }

    const View    *child_ = nullptr;
    layout::Insets padding_{};
    core::Cell     fill_cell_{core::Cell::from_char(U' ')};
    core::Cell     border_cell_{core::Cell::from_char(U'#')};
    bool           draw_fill_   = false;
    bool           draw_border_ = false;
  };

} // namespace glyph::view

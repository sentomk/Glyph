// glyph/view/components/table.h
//
// TableView: render a simple table with fixed/flex columns.
//
// Responsibilities:
//   - Render column headers and rows with horizontal alignment.
//   - Clip text to column width.

#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "glyph/core/cell.h"
#include "glyph/core/geometry.h"
#include "glyph/view/frame.h"
#include "glyph/view/layout/align.h"
#include "glyph/view/layout/box.h"
#include "glyph/view/layout/scroll.h"
#include "glyph/view/view.h"

namespace glyph::view {

  class TableView final : public View {
  public:
    struct Column final {
      std::u32string   title{};
      core::coord_t    width  = -1; // <0 = flex
      core::coord_t    weight = 1;
      layout::AlignH   align  = layout::AlignH::Left;
    };

    using Row = std::vector<std::u32string>;

    explicit TableView(std::vector<Column> columns = {})
        : columns_(std::move(columns)) {
    }

    void set_columns(std::vector<Column> columns) {
      columns_ = std::move(columns);
    }

    void set_rows(std::vector<Row> rows) {
      rows_ = std::move(rows);
    }

    void add_row(Row row) {
      rows_.push_back(std::move(row));
    }

    void clear_rows() {
      rows_.clear();
    }

    void set_show_header(bool enabled) {
      show_header_ = enabled;
    }

    void set_focused(bool focused) {
      focused_ = focused;
    }

    void set_selected_row(core::coord_t row) {
      selected_row_ = row;
    }

    void set_selected_cell(core::Cell cell) {
      selected_cell_ = cell;
      has_selected_cell_ = true;
    }

    void set_unfocused_selected_cell(core::Cell cell) {
      unfocused_selected_cell_ = cell;
      has_unfocused_selected_cell_ = true;
    }

    void set_column_spacing(core::coord_t spacing) {
      spacing_ = std::max<core::coord_t>(0, spacing);
    }

    void set_scroll_offset(core::coord_t offset) {
      scroll_.set_offset(offset);
    }

    void scroll_by(core::coord_t delta) {
      scroll_.scroll_by(delta);
    }

    void scroll_to_start() {
      scroll_.scroll_to_start();
    }

    void scroll_to_end() {
      scroll_.scroll_to_end();
    }

    void set_cell(core::Cell cell) {
      cell_ = cell;
    }

    void set_header_cell(core::Cell cell) {
      header_cell_ = cell;
    }

    void render(Frame &f, core::Rect area) const override {
      if (area.empty() || columns_.empty()) {
        return;
      }

      std::vector<layout::BoxItem> items;
      items.reserve(columns_.size());
      for (const auto &col : columns_) {
        layout::BoxItem item{};
        if (col.width >= 0) {
          item.main = col.width;
          item.flex = 0;
        }
        else {
          item.main = -1;
          item.flex = std::max<core::coord_t>(1, col.weight);
        }
        items.push_back(item);
      }

      const auto layout_out =
          layout::layout_box(layout::Axis::Horizontal, area, items, spacing_);
      if (layout_out.rects.empty()) {
        return;
      }

      const auto max_cols = std::min(layout_out.rects.size(), columns_.size());
      core::coord_t y = area.top();

      if (show_header_ && y < area.bottom()) {
        for (std::size_t i = 0; i < max_cols; ++i) {
          const auto rect = row_rect(layout_out.rects[i], y);
          render_cell(f, rect, columns_[i].title, columns_[i].align,
                      header_cell_);
        }
        y = core::coord_t(y + 1);
      }

      const core::coord_t available_rows =
          std::max<core::coord_t>(0, core::coord_t(area.bottom() - y));
      const auto scroll = make_scroll(available_rows);

      const auto start = std::max<core::coord_t>(0, scroll.visible_start());
      const auto end = std::min<core::coord_t>(
          static_cast<core::coord_t>(rows_.size()), scroll.visible_end());
      core::coord_t row_y = y;
      for (core::coord_t row = start; row < end; ++row) {
        const auto &cells = rows_[static_cast<std::size_t>(row)];
        const bool  selected = (row == selected_row_);
        for (std::size_t col = 0; col < max_cols; ++col) {
          const auto rect = row_rect(layout_out.rects[col], row_y);
          const std::u32string_view text =
              (col < cells.size()) ? std::u32string_view(cells[col])
                                   : std::u32string_view{};
          core::Cell cell = cell_;
          if (selected) {
            if (focused_ && has_selected_cell_) {
              cell = selected_cell_;
            }
            else if (!focused_ && has_unfocused_selected_cell_) {
              cell = unfocused_selected_cell_;
            }
          }
          render_cell(f, rect, text, columns_[col].align, cell);
        }
        row_y = core::coord_t(row_y + 1);
      }
    }

  private:
    static core::Rect row_rect(core::Rect col, core::coord_t y) {
      return core::Rect{core::Point{col.left(), y},
                        core::Size{col.size.w, 1}};
    }

    static core::coord_t text_width(std::u32string_view text) {
      core::coord_t w = 0;
      for (char32_t ch : text) {
        w = core::coord_t(w + core::cell_width(ch));
      }
      return w;
    }

    static void render_cell(Frame &f, core::Rect area,
                            std::u32string_view text, layout::AlignH align,
                            core::Cell cell) {
      if (area.empty() || area.size.w <= 0) {
        return;
      }

      core::coord_t x = area.left();
      const core::coord_t available = area.size.w;
      const core::coord_t width = text_width(text);

      if (width <= available) {
        switch (align) {
        case layout::AlignH::Center:
          x = core::coord_t(area.left() + (available - width) / 2);
          break;
        case layout::AlignH::Right:
          x = core::coord_t(area.right() - width);
          break;
        case layout::AlignH::Left:
        case layout::AlignH::Stretch:
        default:
          x = area.left();
          break;
        }
      }

      for (char32_t ch : text) {
        const core::coord_t w = core::coord_t(core::cell_width(ch));
        if (w <= 0) {
          continue;
        }
        if (x + w > area.right()) {
          break;
        }
        core::Cell out = cell;
        out.ch = ch;
        out.width = static_cast<std::uint8_t>(w);
        f.set(core::Point{x, area.top()}, out);
        x = core::coord_t(x + w);
      }
    }

    layout::ScrollModel make_scroll(core::coord_t viewport) const {
      layout::ScrollModel scroll = scroll_;
      scroll.set_content(static_cast<core::coord_t>(rows_.size()));
      scroll.set_viewport(viewport);
      return scroll;
    }

    std::vector<Column> columns_{};
    std::vector<Row>    rows_{};
    core::Cell          cell_{core::Cell::from_char(U' ')};
    core::Cell          header_cell_{core::Cell::from_char(U' ')};
    core::Cell          selected_cell_{core::Cell::from_char(U' ')};
    core::Cell          unfocused_selected_cell_{core::Cell::from_char(U' ')};
    layout::ScrollModel scroll_{};
    core::coord_t       spacing_ = 1;
    bool                show_header_ = true;
    bool                focused_ = false;
    core::coord_t       selected_row_ = -1;
    bool                has_selected_cell_ = false;
    bool                has_unfocused_selected_cell_ = false;
  };

} // namespace glyph::view

// glyph/view/components/box_layout.h
//
// BoxLayoutView: compose child Views in a linear box layout.
//
// Responsibilities:
//   - Use layout::layout_box() to compute child rects.
//   - Render children in order along the chosen axis.
//   - Provide HBox/VBox helpers for common usage.

#pragma once

#include <initializer_list>
#include <utility>
#include <vector>

#include "glyph/core/geometry.h"
#include "glyph/view/frame.h"
#include "glyph/view/layout/box.h"
#include "glyph/view/view.h"

namespace glyph::view {

  // ------------------------------------------------------------
  // BoxChild
  // ------------------------------------------------------------
  struct BoxChild final {
    const View   *view   = nullptr;
    core::coord_t main   = -1;
    core::coord_t weight = 1;
  };

  // ------------------------------------------------------------
  // BoxLayoutView
  // ------------------------------------------------------------
  class BoxLayoutView : public View {

  public:
    explicit BoxLayoutView(layout::Axis                    axis,
                           std::initializer_list<BoxChild> children,
                           core::coord_t                   spacing = 0)
        : axis_(axis), spacing_(spacing), children_(children) {
    }

    explicit BoxLayoutView(layout::Axis axis, std::vector<BoxChild> children,
                           core::coord_t spacing = 0)
        : axis_(axis), spacing_(spacing), children_(std::move(children)) {
    }

    void render(Frame &f, core::Rect area) const override {
      if (area.empty() || children_.empty()) {
        return;
      }

      std::vector<layout::BoxItem> items;
      items.reserve(children_.size());
      for (const auto &child : children_) {
        layout::BoxItem item{};
        item.main = child.main;
        item.flex = child.weight;
        items.push_back(item);
      }

      const auto out = layout::layout_box(axis_, area, items, spacing_);
      const auto count = std::min(out.rects.size(), children_.size());
      for (std::size_t i = 0; i < count; ++i) {
        const auto *view = children_[i].view;
        if (view == nullptr) {
          continue;
        }
        view->render(f, out.rects[i]);
      }
    }

  private:
    layout::Axis          axis_;
    core::coord_t         spacing_ = 0;
    std::vector<BoxChild> children_{};
  };

  // ------------------------------------------------------------
  // HBox / VBox helpers
  // ------------------------------------------------------------
  inline BoxLayoutView HBox(std::initializer_list<BoxChild> children,
                            core::coord_t                   spacing = 0) {
    return BoxLayoutView(layout::Axis::Horizontal, children, spacing);
  }

  inline BoxLayoutView VBox(std::initializer_list<BoxChild> children,
                            core::coord_t                   spacing = 0) {
    return BoxLayoutView(layout::Axis::Vertical, children, spacing);
  }

} // namespace glyph::view
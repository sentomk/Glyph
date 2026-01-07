// glyph/view/components/stack.h
//
// Stack: compose child Views in a linear box layout.
//
// Responsibilities:
//   - Use layout::layout_box() to compute child rects.
//   - Render children in order along the chosen axis.
//   - Provide HStack/VStack helpers for common usage.

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
  // StackChild
  // ------------------------------------------------------------
  struct StackChild final {
    const View   *view   = nullptr;
    core::coord_t main   = -1;
    core::coord_t weight = 1;
  };

  // ------------------------------------------------------------
  // StackChild helpers
  // ------------------------------------------------------------
  inline StackChild Fixed(const View *view, core::coord_t main) {
    return StackChild{.view = view, .main = main, .weight = 0};
  }

  inline StackChild Fixed(const View &view, core::coord_t main) {
    return Fixed(&view, main);
  }

  inline StackChild Flex(const View *view, core::coord_t weight = 1) {
    return StackChild{.view = view, .main = -1, .weight = weight};
  }

  inline StackChild Flex(const View &view, core::coord_t weight = 1) {
    return Flex(&view, weight);
  }

  // ------------------------------------------------------------
  // Stack
  // ------------------------------------------------------------
  class Stack : public View {

  public:
    explicit Stack(layout::Axis                     axis,
                   std::initializer_list<StackChild> children,
                   core::coord_t                    spacing = 0)
        : axis_(axis), spacing_(spacing), children_(children) {
    }

    explicit Stack(layout::Axis axis, std::vector<StackChild> children,
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
    layout::Axis            axis_;
    core::coord_t           spacing_ = 0;
    std::vector<StackChild> children_{};
  };

  // ------------------------------------------------------------
  // ZStack
  // ------------------------------------------------------------
  // Overlay children in order within the same rect.
  class ZStack : public View {
  public:
    explicit ZStack(std::initializer_list<const View *> children)
        : children_(children) {
    }

    explicit ZStack(std::vector<const View *> children)
        : children_(std::move(children)) {
    }

    void render(Frame &f, core::Rect area) const override {
      if (area.empty() || children_.empty()) {
        return;
      }

      for (const auto *view : children_) {
        if (view == nullptr) {
          continue;
        }
        view->render(f, area);
      }
    }

  private:
    std::vector<const View *> children_{};
  };

  // ------------------------------------------------------------
  // HStack / VStack helpers
  // ------------------------------------------------------------
  inline Stack HStack(std::initializer_list<StackChild> children,
                      core::coord_t                    spacing = 0) {
    return Stack(layout::Axis::Horizontal, children, spacing);
  }

  inline Stack VStack(std::initializer_list<StackChild> children,
                      core::coord_t                    spacing = 0) {
    return Stack(layout::Axis::Vertical, children, spacing);
  }

  inline Stack HStack(std::initializer_list<const View *> children,
                      core::coord_t                      spacing = 0) {
    std::vector<StackChild> items;
    items.reserve(children.size());
    for (const auto *view : children) {
      items.push_back(Flex(view));
    }
    return Stack(layout::Axis::Horizontal, std::move(items), spacing);
  }

  inline Stack VStack(std::initializer_list<const View *> children,
                      core::coord_t                      spacing = 0) {
    std::vector<StackChild> items;
    items.reserve(children.size());
    for (const auto *view : children) {
      items.push_back(Flex(view));
    }
    return Stack(layout::Axis::Vertical, std::move(items), spacing);
  }

  // ------------------------------------------------------------
  // ZStack helper
  // ------------------------------------------------------------
  inline ZStack ZStackView(std::initializer_list<const View *> children) {
    return ZStack(children);
  }

} // namespace glyph::view

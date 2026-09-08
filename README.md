# Glyph

Glyph is a modern C++ TUI (Terminal UI) engine prototype. It provides a clear *semantic 2D canvas* abstraction and pluggable rendering backends. Current status is **0.3.0 prototype**, and APIs may change.

## Goals & Positioning

- **Goal**: provide a stable data model and rendering pipeline as a foundation for future UI components and layout systems
- **Positioning**: engine-level infrastructure (not a full UI framework)

## Architecture

```
View → Frame (semantic draw) → Renderer (backend output)
```

- **core/**: geometry / Cell / Buffer / width rules / dirty / diff
- **view/**: semantic drawing interfaces (View, Frame, Canvas)
- **view/layout/**: pure layout helpers (box/stack/inset/align/split/scroll)
- **view/components/**: Label/Panel/Bar/Table/Focus
- **render/**: backend output (ANSI / Debug)
- **input/**: unified event model + pluggable platform backends (Windows / POSIX)

Layering rule: upper layers do not depend on lower-level details; backends are replaceable.

## Current Features (0.3.0)
- Semantic 2D canvas and minimal draw pipeline
- `Cell` width policy + write-time dirty marking
- ANSI renderer (diff/dirty optimized, true-color styles)
- Cross-platform input (Windows + POSIX: shared VT decoder, key/mouse/paste events)
- Layout helpers: box/stack/inset/align/split + scroll model
- Components: LabelView, PanelView, BarView, TableView, TextInputView
- Focus/selection models for list/table style components
- Demos: aurora_dashboard, agent_chat, components_demo, bar_demo, poll_stress_demo, snake_demo

## Example
Build & run:
```
cmake -S . -B build
cmake --build build
./build/ansi_render
```

Layout demo:
```
./build/layout_demo
```

Exit with `Esc` or `Q`.

Component demo:
```
./build/components_demo
```

UI demo:
```
./build/aurora_dashboard
```

## Install (CMake)
```
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/path/to/install
cmake --build build
cmake --install build
```

Then use:
```
find_package(glyph CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE glyph::glyph)
```

## Directory Layout
```
include/glyph/
  core/    core types, geometry, Cell, Buffer, diff
  view/    View/Frame/Canvas
  view/layout/  box/stack/inset/align/split/scroll
  view/components/  Label/Panel/Bar/Table/TextInput/Focus
  render/  Renderer/ANSI/Debug
  input/   Event/Input/WinInput/PosixInput
src/
samples/
```

## Design Constraints
- **Canvas semantics**: draw by writing Cell grid, no direct control codes
- **Pluggable backends**: Renderer decoupled from View
- **Lightweight core**: keep core types small and stable

## Known Limitations (current)
- TableView is basic (no virtualization or column resizing)
- No chart/graph components yet
- TextInputView is single-line only

## Roadmap
- Charts (line chart, progress bar)
- System stats (CPU/memory/process list)
- Glyph-Top demo (table + charts + status)

## Dev Guide (quick)
### Add a renderer backend
1) Implement `glyph::render::Renderer` in `src/`.
2) Add the header under `include/glyph/render/`.
3) Register the source in `CMakeLists.txt`.
4) Create a small sample in `samples/` to validate output.

### Add an input backend
1) Implement `glyph::input::Input` (poll/read/set_mode/get_mode).
2) Translate platform input into `glyph::core::Event`.
3) Add the header under `include/glyph/input/` and source in `src/`.

### Add a view / component
1) Implement `glyph::view::View::render(Frame&, Rect)`.
2) Use `Frame::set()` / `Canvas::set()` for drawing.
3) Keep rendering IO in render layer only.

## Contributing
Prototype APIs may change. Contributions to core/render/input and samples are welcome.

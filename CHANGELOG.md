# Changelog

Notable changes per release. Earlier versions predate this file; 0.4.0 is the
first tagged release.

## 0.4.0

### Added

- **Content-anchored selection in `ScrollRegionView`** — a selection is stored
  against logical lines rather than frame cells, so it survives scrolling,
  re-wrapping and new output; `extract_selection()` reads rows that have since
  scrolled out of view. Groundwork for tmux-grade copy.
- **Mouse wheel scrolling in `ScrollRegionView`**, with a configurable step
  and edge-drag auto-scroll.
- **`ScrollRegionView::replace_last_line`** for streaming updates: growing text
  re-wraps only its own rows, so a reader who scrolled up is not yanked.
- **`SelectionModel`** with flow and block semantics, and reverse-video
  highlighting over a rendered frame.
- **Word-wrapped scrollback** — long lines wrap into visual rows instead of
  breaking mid-word, without splitting wide glyphs.

### Fixed

- Selection hit-testing ignored the render area's left origin.
- Scrolling down did nothing for several notches after wheeling past the
  oldest row; the scroll offset is now normalized at render.
- Wheel sensitivity raised to five rows per notch (three per edge-drag).
- The alternate-screen guard restores the tty when a fatal signal kills a
  raw-mode application.

### Changed — build and packaging

- **texere is looked for before it is fetched**, and pinned to a released tag
  with a shallow clone. A consumer that already has it — vcpkg, Conan, a
  distribution package — configures without touching the network.
- **The installed package links.** glyph is a static library, so texere is in
  its link interface; the reference is now exported when texere came from a
  package (with `find_dependency` in `glyphConfig.cmake`) and stays build-tree
  only when texere was fetched, which is the one case CMake can export.
- Glyph no longer configures texere's build switches. A library owns how its
  dependencies are obtained.

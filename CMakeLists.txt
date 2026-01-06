cmake_minimum_required(VERSION 3.16...3.28)

project(glyph
  VERSION 0.1.2
  DESCRIPTION "TUI engine for modern C++"
  LANGUAGES CXX 
)

# ------------------------------------------------------------
# Language & Tooling
# ------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

# ------------------------------------------------------------
# Utils
# ------------------------------------------------------------

function(GLYPH_JOIN_HEADERS OUT_VAR)
  set(_accum)
  
  foreach(_h IN LISTS ARGN)
    list(APPEND _accum "${_h}")
  endforeach()

  set(${OUT_VAR} "${_accum}" PARENT_SCOPE)
endfunction()

# ------------------------------------------------------------
# Public Headers
# ------------------------------------------------------------

GLYPH_JOIN_HEADERS(GLYPH_PUBLIC_HEADERS

  # core/
  include/glyph/core/buffer.h
  include/glyph/core/cell.h
  include/glyph/core/diff.h
  include/glyph/core/event.h
  include/glyph/core/geometry.h
  include/glyph/core/style.h
  include/glyph/core/types.h
  include/glyph/core/text.h

  # input/
  include/glyph/input/input.h
  include/glyph/input/input_guard.h
  include/glyph/input/win32/win_input.h

  # render/
  include/glyph/render/render.h
  include/glyph/render/terminal.h
  include/glyph/render/ansi/ansi_renderer.h
  include/glyph/render/debug/debug_renderer.h


  # view/
  include/glyph/view/canvas.h
  include/glyph/view/frame.h
  include/glyph/view/text.h
  include/glyph/view/view.h
  # view/components
  include/glyph/view/components/border.h
  include/glyph/view/components/fill.h
  include/glyph/view/components/inset.h
  include/glyph/view/components/label.h
  include/glyph/view/components/panel.h
  include/glyph/view/components/stack.h



  # view/layout
  include/glyph/view/layout/align.h
  include/glyph/view/layout/box.h
  include/glyph/view/layout/inset.h
  include/glyph/view/layout/stack.h
  include/glyph/view/layout/split.h
  include/glyph/view/layout/types.h

)

# ------------------------------------------------------------
# Core Library
# ------------------------------------------------------------

add_library(glyph
  src/render/terminal.cpp
  src/render/debug/debug_renderer.cpp
  src/render/ansi/ansi_renderer.cpp
  src/win32/win_input.cpp
)

add_library(glyph::glyph ALIAS glyph)

# language/features
target_compile_features(glyph PUBLIC cxx_std_20)

# include paths
target_include_directories(glyph
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# public headers (IDE / install awareness)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.23)
  target_sources(glyph
    PUBLIC
      FILE_SET HEADERS
      BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
      FILES ${GLYPH_PUBLIC_HEADERS}
  )
endif()

# ------------------------------------------------------------
# Samples
# ------------------------------------------------------------

# debug_render
add_executable(debug_render
  samples/basic/debug_render.cpp
)
target_link_libraries(debug_render
  PRIVATE glyph::glyph
)

# ansi_render
add_executable(ansi_render
  samples/basic/ansi_render.cpp
)
target_link_libraries(ansi_render
  PRIVATE glyph::glyph
)

# layout_demo
add_executable(layout_demo
  samples/basic/layout_demo.cpp
)
target_link_libraries(layout_demo
  PRIVATE glyph::glyph
)


# layout_gallery
add_executable(layout_gallery
  samples/basic/layout_gallery.cpp
)
target_link_libraries(layout_gallery
  PRIVATE glyph::glyph
)


# components_demo
add_executable(components_demo
  samples/basic/components_demo.cpp
)
target_link_libraries(components_demo
  PRIVATE glyph::glyph
)

# snake_demo
add_executable(snake_demo
  samples/basic/snake_demo.cpp
)
target_link_libraries(snake_demo
  PRIVATE glyph::glyph
)

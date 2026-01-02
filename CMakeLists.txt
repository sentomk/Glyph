cmake_minimum_required(VERSION 3.16...3.28)

project(glyph
  VERSION 0.1.0
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
  include/glyph/core/geometry.h
  include/glyph/core/style.h
  include/glyph/core/types.h
  include/glyph/core/text.h

  # view/
  include/glyph/view/frame.h
  include/glyph/view/view.h
  include/glyph/view/canvas.h
  include/glyph/view/primitives/fill.h

  # render/
  include/glyph/render/render.h
  include/glyph/render/ansi/ansi_renderer.h
  include/glyph/render/debug/debug_renderer.h

)

# ------------------------------------------------------------
# Core Library
# ------------------------------------------------------------

add_library(glyph
  src/debug_renderer.cpp
  src/ansi_renderer.cpp
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

add_executable(debug_render
  samples/debug_render.cpp
)
target_link_libraries(debug_render
  PRIVATE glyph::glyph
)
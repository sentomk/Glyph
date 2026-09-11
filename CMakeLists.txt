cmake_minimum_required(VERSION 3.16...3.28)

project(glyph
  VERSION 0.3.0
  DESCRIPTION "TUI engine for modern C++"
  LANGUAGES CXX
)

# ------------------------------------------------------------
# Language & Tooling
# ------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sources and tests embed UTF-8 literals (CJK/emoji/box-drawing), and the
# library treats char data as UTF-8 at runtime. Declare both charsets for
# MSVC, which otherwise assumes the system codepage.
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

# ------------------------------------------------------------
# Grapheme segmentation backend
# ------------------------------------------------------------
# texere provides UAX #29 segmentation (and UAX #11 / UTS #51 width) with
# the heavy unicode backends compiled behind its own library boundary.
# It is a PRIVATE dependency: core headers never include texere types.
option(GLYPH_USE_TEXERE "Use texere for UAX #29 grapheme segmentation" ON)

if(GLYPH_USE_TEXERE)
  # Look first, download only when it is missing: a consumer that already has
  # texere — vcpkg, Conan, a distribution package — configures without touching
  # the network, and one that forbids downloads gets a sentence saying what is
  # wrong instead of a surprise clone. Set CMAKE_DISABLE_FIND_PACKAGE_texere=ON
  # to insist on the pinned revision instead.
  find_package(texere CONFIG QUIET)
  if(NOT texere_FOUND)
    include(FetchContent)
    # texere switches its own tests and tools off when it is embedded, so these
    # three lines go away with the next pin bump; they are here because the
    # pinned revision predates that.
    set(TEXERE_BUILD_TESTS      OFF CACHE BOOL "" FORCE)
    set(TEXERE_BUILD_TOOLS      OFF CACHE BOOL "" FORCE)
    set(TEXERE_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(texere
      GIT_REPOSITORY https://github.com/sentomk/texere.git
      GIT_TAG        ecfc6cab1e004dc0391a9763c20be969705edcf9
    )
    FetchContent_MakeAvailable(texere)
  endif()
  if(NOT TARGET texere::texere)
    message(FATAL_ERROR
      "GLYPH_USE_TEXERE=ON but texere is not available: find_package did not "
      "find it and FetchContent did not populate it. Install it, put its prefix "
      "on CMAKE_PREFIX_PATH, or configure with -DGLYPH_USE_TEXERE=OFF to use "
      "glyph's own segmentation.")
  endif()
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GLYPH_BUILD_TESTS "Build Glyph tests" ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

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
  include/glyph/core/color.h
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
  include/glyph/input/detail/vt_decoder.h
  include/glyph/input/win32/win_input.h
  include/glyph/input/posix/posix_input.h

  # platform/
  include/glyph/platform/clipboard.h

  # render/
  include/glyph/render/render.h
  include/glyph/render/terminal.h
  include/glyph/render/ansi/ansi_renderer.h
  include/glyph/render/debug/debug_renderer.h


  # view/
  include/glyph/view/canvas.h
  include/glyph/view/frame.h
  include/glyph/view/selection.h
  include/glyph/view/text.h
  include/glyph/view/view.h
  # view/components
  include/glyph/view/components/border.h
  include/glyph/view/components/bar.h
  include/glyph/view/components/fill.h
  include/glyph/view/components/focus.h
  include/glyph/view/components/inset.h
  include/glyph/view/components/label.h
  include/glyph/view/components/panel.h
  include/glyph/view/components/stack.h
  include/glyph/view/components/table.h
  include/glyph/view/components/text_input.h
  include/glyph/view/components/scroll_region.h
  include/glyph/view/components/status_line.h
  # view/layout
  include/glyph/view/layout/align.h
  include/glyph/view/layout/box.h
  include/glyph/view/layout/inset.h
  include/glyph/view/layout/scroll.h
  include/glyph/view/layout/split.h
  include/glyph/view/layout/types.h

)

# ------------------------------------------------------------
# Core Library
# ------------------------------------------------------------

# Platform-specific input backend.
if(WIN32)
  set(GLYPH_INPUT_BACKEND src/win32/win_input.cpp)
else()
  set(GLYPH_INPUT_BACKEND src/posix/posix_input.cpp)
endif()

add_library(glyph
  src/render/terminal.cpp
  src/render/debug/debug_renderer.cpp
  src/render/ansi/ansi_renderer.cpp
  src/input/vt_decoder.cpp
  src/input/make_input.cpp
  src/text_segment.cpp
  src/platform/clipboard.cpp
  ${GLYPH_INPUT_BACKEND}
)

add_library(glyph::glyph ALIAS glyph)

if(GLYPH_USE_TEXERE)
  # $<BUILD_INTERFACE:> keeps texere out of the install(EXPORT) interface
  # (it is not in any export set). The installed static library still
  # carries texere symbols; wiring the package-level dependency is deferred
  # until texere ships find_package support.
  target_link_libraries(glyph PRIVATE $<BUILD_INTERFACE:texere::texere>)
  target_compile_definitions(glyph PRIVATE GLYPH_USE_TEXERE=1)
endif()

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
# Install (CMake package)
# ------------------------------------------------------------

set(GLYPH_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/glyph")

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.23)
  install(TARGETS glyph
    EXPORT glyphTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
else()
  install(TARGETS glyph
    EXPORT glyphTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
  install(DIRECTORY include/glyph
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
endif()

install(EXPORT glyphTargets
  NAMESPACE glyph::
  DESTINATION ${GLYPH_INSTALL_CMAKEDIR}
)

configure_package_config_file(
  cmake/glyphConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/glyphConfig.cmake
  INSTALL_DESTINATION ${GLYPH_INSTALL_CMAKEDIR}
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/glyphConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/glyphConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/glyphConfigVersion.cmake
  DESTINATION ${GLYPH_INSTALL_CMAKEDIR}
)

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

# bar_demo
add_executable(bar_demo
  samples/basic/bar_demo.cpp
)
target_link_libraries(bar_demo
  PRIVATE glyph::glyph
)

# ui_demo
add_executable(aurora_dashboard
  samples/ui/aurora_dashboard.cpp
)
target_link_libraries(aurora_dashboard
  PRIVATE glyph::glyph
)

# snake_demo
add_executable(snake_demo
  samples/basic/snake_demo.cpp
)
target_link_libraries(snake_demo
  PRIVATE glyph::glyph
)

# poll_stress_demo
add_executable(poll_stress_demo
  samples/basic/poll_stress_demo.cpp
)
target_link_libraries(poll_stress_demo
  PRIVATE glyph::glyph
)

# agent_chat
add_executable(agent_chat
  samples/ui/agent_chat.cpp
)
target_link_libraries(agent_chat
  PRIVATE glyph::glyph
)

# selection_demo
add_executable(selection_demo
  samples/ui/selection_demo.cpp
)
target_link_libraries(selection_demo
  PRIVATE glyph::glyph
)

# ------------------------------------------------------------
# Tests
# ------------------------------------------------------------

if(GLYPH_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ------------------------------------------------------------
# doctest (fetched)
# ------------------------------------------------------------

include(FetchContent)

# doctest v2.4.11 still declares an ancient cmake_minimum_required that
# CMake >= 4.0 rejects. Allow its subproject to configure under the old
# policy floor without touching the fetched source.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        v2.4.11
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(doctest)

list(APPEND CMAKE_MODULE_PATH ${doctest_SOURCE_DIR}/scripts/cmake)
include(doctest)

# ------------------------------------------------------------
# Shared doctest main (compiled once, linked by every test target)
# ------------------------------------------------------------

add_library(glyph_test_main STATIC doctest_main.cpp)
target_link_libraries(glyph_test_main PUBLIC doctest::doctest)

# ------------------------------------------------------------
# Helper: declare a test executable + register with CTest
# ------------------------------------------------------------

function(glyph_add_test NAME)
  add_executable(${NAME} ${ARGN})
  target_link_libraries(${NAME}
    PRIVATE glyph::glyph glyph_test_main doctest::doctest
  )
  doctest_discover_tests(${NAME})
endfunction()

# ------------------------------------------------------------
# Test targets
# ------------------------------------------------------------

glyph_add_test(test_geometry       unit/test_geometry.cpp)
glyph_add_test(test_cell_width     unit/test_cell_width.cpp)
glyph_add_test(test_buffer         unit/test_buffer.cpp)
glyph_add_test(test_diff           unit/test_diff.cpp)
glyph_add_test(test_vt_decoder     unit/test_vt_decoder.cpp)
glyph_add_test(test_render_pipeline integration/test_render_pipeline.cpp)
glyph_add_test(test_input_stream   e2e/test_input_stream.cpp)
glyph_add_test(test_ansi_output    snapshot/test_ansi_output.cpp)

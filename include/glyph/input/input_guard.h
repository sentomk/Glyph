// glyph/input/input_guard.h
//
// RAII helper for input mode management.
//
// Responsibilities:
//   - Enable a desired input mode on construction.
//   - Restore previous mode on destruction.

#pragma once

#include "input.h"

namespace glyph::input {

  class InputGuard final {
  public:
    InputGuard(Input &input, InputMode mode)
        : input_(input), prev_(input.get_mode()) {
      input_.set_mode(mode);
    }

    ~InputGuard() {
      input_.set_mode(prev_);
    }

    InputGuard(const InputGuard &)            = delete;
    InputGuard &operator=(const InputGuard &) = delete;

  private:
    Input    &input_;
    InputMode prev_;
  };

} // namespace glyph::input
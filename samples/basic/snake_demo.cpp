// samples/basic/snake_demo.cpp
//
// Simple Snake game demo using the Glyph renderer and WinInput.

#include <algorithm>
#include <chrono>
#include <deque>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "glyph/core/cell.h"
#include "glyph/core/event.h"
#include "glyph/core/geometry.h"
#include "glyph/core/style.h"
#include "glyph/input/win32/win_input.h"
#include "glyph/render/terminal.h"
#include "glyph/view/components/panel.h"
#include "glyph/view/components/stack.h"
#include "glyph/view/frame.h"
#include "glyph/view/layout/inset.h"
#include "glyph/view/text.h"
#include "glyph/view/view.h"

namespace {

  enum class Dir {
    Up,
    Down,
    Left,
    Right,
  };

  struct GameState {
    std::deque<glyph::core::Point>  snake{};
    glyph::core::Point              food{};
    std::vector<glyph::core::Point> obstacles{};
    Dir                             dir     = Dir::Right;
    bool                            alive   = true;
    bool                            paused  = false;
    int                             score   = 0;
    int                             apples  = 0;
    int                             tick_ms = 120;
  };

  struct Grid {
    glyph::core::coord_t w = 0;
    glyph::core::coord_t h = 0;
  };

  constexpr glyph::core::coord_t        kStatusHeight = 1;
  constexpr glyph::view::layout::Insets kGamePadding =
      glyph::view::layout::Insets::all(1);

  glyph::core::Point step(glyph::core::Point p, Dir d) {
    using glyph::core::coord_t;
    switch (d) {
    case Dir::Up:
      return {p.x, coord_t(p.y - 1)};
    case Dir::Down:
      return {p.x, coord_t(p.y + 1)};
    case Dir::Left:
      return {coord_t(p.x - 1), p.y};
    case Dir::Right:
      return {coord_t(p.x + 1), p.y};
    }
    return p;
  }

  bool is_opposite(Dir a, Dir b) {
    return (a == Dir::Up && b == Dir::Down) ||
           (a == Dir::Down && b == Dir::Up) ||
           (a == Dir::Left && b == Dir::Right) ||
           (a == Dir::Right && b == Dir::Left);
  }

  bool
  contains(const std::deque<glyph::core::Point> &snake, glyph::core::Point p) {
    return std::find(snake.begin(), snake.end(), p) != snake.end();
  }

  bool
  contains(const std::vector<glyph::core::Point> &items, glyph::core::Point p) {
    return std::find(items.begin(), items.end(), p) != items.end();
  }

  bool is_blocked(
      const std::deque<glyph::core::Point>  &snake,
      const std::vector<glyph::core::Point> &obstacles,
      glyph::core::Point                     p) {
    return contains(snake, p) || contains(obstacles, p);
  }

  glyph::core::Point random_empty_cell(
      std::mt19937                          &rng,
      const Grid                            &grid,
      const std::deque<glyph::core::Point>  &snake,
      const std::vector<glyph::core::Point> &obstacles) {
    std::uniform_int_distribution<int> dist_x(0, grid.w - 1);
    std::uniform_int_distribution<int> dist_y(0, grid.h - 1);

    for (int attempts = 0; attempts < 256; ++attempts) {
      glyph::core::Point p{
          static_cast<glyph::core::coord_t>(dist_x(rng)),
          static_cast<glyph::core::coord_t>(dist_y(rng))};
      if (!is_blocked(snake, obstacles, p)) {
        return p;
      }
    }

    for (glyph::core::coord_t y = 0; y < grid.h; ++y) {
      for (glyph::core::coord_t x = 0; x < grid.w; ++x) {
        glyph::core::Point p{x, y};
        if (!is_blocked(snake, obstacles, p)) {
          return p;
        }
      }
    }

    return {0, 0};
  }

  std::vector<glyph::core::Point> make_obstacles(
      std::mt19937                         &rng,
      const Grid                           &grid,
      const std::deque<glyph::core::Point> &snake,
      glyph::core::coord_t                  count) {
    std::vector<glyph::core::Point> out;
    out.reserve(static_cast<std::size_t>(count));

    for (glyph::core::coord_t i = 0; i < count; ++i) {
      const auto p = random_empty_cell(rng, grid, snake, out);
      if (contains(out, p) || contains(snake, p)) {
        break;
      }
      out.push_back(p);
    }

    return out;
  }

  int next_tick_ms(int base_ms, int apples) {
    const int speedup_every = 5;
    const int step_ms       = 8;
    const int min_ms        = 24;
    const int boosts        = apples / speedup_every;
    const int adjusted      = base_ms - boosts * step_ms;
    return std::max(min_ms, adjusted);
  }

  void reset_game(GameState &state, const Grid &grid, std::mt19937 &rng) {
    state.snake.clear();
    state.score   = 0;
    state.apples  = 0;
    state.dir     = Dir::Right;
    state.alive   = true;
    state.paused  = false;
    state.tick_ms = 90;

    const auto cx = grid.w / 2;
    const auto cy = grid.h / 2;
    state.snake.push_back({cx, cy});
    state.snake.push_back({glyph::core::coord_t(cx - 1), cy});
    state.snake.push_back({glyph::core::coord_t(cx - 2), cy});

    const auto max_cells = grid.w * grid.h;
    const auto target_obstacles =
        std::max<glyph::core::coord_t>(1, max_cells / 80);
    state.obstacles = make_obstacles(rng, grid, state.snake, target_obstacles);
    state.food = random_empty_cell(rng, grid, state.snake, state.obstacles);
  }

  Grid grid_from_frame(glyph::core::Size size) {
    const auto game_h = glyph::core::coord_t(size.h - kStatusHeight);
    const auto inner_w =
        glyph::core::coord_t(size.w - (kGamePadding.left + kGamePadding.right));
    const auto inner_h =
        glyph::core::coord_t(game_h - (kGamePadding.top + kGamePadding.bottom));
    return Grid{
        std::max<glyph::core::coord_t>(0, inner_w),
        std::max<glyph::core::coord_t>(0, inner_h),
    };
  }

  class ScoreBoardView final : public glyph::view::View {
  public:
    explicit ScoreBoardView(const GameState *state) : state_(state) {
    }

    void render(glyph::view::Frame &f, glyph::core::Rect area) const override {
      if (state_ == nullptr || area.empty()) {
        return;
      }

      auto        canvas = f.sub_frame(area);
      std::string status = "Score: " + std::to_string(state_->score) +
                           "  Speed: " + std::to_string(state_->tick_ms) +
                           "ms  [Arrows/WASD]  P:Pause  R:Reset  Q/Esc:Quit";
      if (!state_->alive) {
        status += "  GAME OVER";
      }
      glyph::view::draw_text(canvas, {0, 0}, status, glyph::core::Cell(U' '));
    }

  private:
    const GameState *state_ = nullptr;
  };

  class SnakeView final : public glyph::view::View {
  public:
    explicit SnakeView(const GameState *state) : state_(state) {
    }

    void render(glyph::view::Frame &f, glyph::core::Rect area) const override {
      if (state_ == nullptr || area.empty()) {
        return;
      }

      auto canvas = f.sub_frame(area);
      if (canvas.empty()) {
        return;
      }

      const auto snake_style =
          glyph::core::Style{}.fg(glyph::core::colors::Lime);
      const auto head_style =
          glyph::core::Style{}.fg(glyph::core::colors::Gold);
      const auto food_style =
          glyph::core::Style{}.fg(glyph::core::colors::Red);
      const auto obstacle_style =
          glyph::core::Style{}.fg(glyph::core::colors::DarkGray);

      const auto head_cell     = glyph::core::Cell(U'O', head_style);
      const auto body_cell     = glyph::core::Cell(U'o', snake_style);
      const auto food_cell     = glyph::core::Cell(U'*', food_style);
      const auto obstacle_cell = glyph::core::Cell(U'X', obstacle_style);

      for (std::size_t i = 0; i < state_->snake.size(); ++i) {
        const auto &seg = state_->snake[i];
        canvas.set({seg.x, seg.y}, i == 0 ? head_cell : body_cell);
      }

      for (const auto &obs : state_->obstacles) {
        canvas.set({obs.x, obs.y}, obstacle_cell);
      }

      canvas.set({state_->food.x, state_->food.y}, food_cell);
    }

  private:
    const GameState *state_ = nullptr;
  };

} // namespace

int main() {
  using namespace glyph;
  using namespace std::chrono_literals;

  render::TerminalApp app{std::cout};
  input::WinInput     input{};
  input.set_mode(input::InputMode::Raw);

  std::mt19937 rng(
      static_cast<std::uint32_t>(
          std::chrono::steady_clock::now().time_since_epoch().count()));

  render::TerminalSize last{};
  GameState            state{};
  bool                 dirty       = true;
  bool                 initialized = false;

  auto next_tick = std::chrono::steady_clock::now();

  for (;;) {
    const auto term   = app.size();
    const auto width  = term.valid ? term.cols : 80;
    const auto height = term.valid ? term.rows : 24;

    if (width <= 0 || height <= 0) {
      std::this_thread::sleep_for(20ms);
      continue;
    }

    const bool size_changed =
        width != last.cols || height != last.rows || term.valid != last.valid;
    if (size_changed) {
      last.cols   = width;
      last.rows   = height;
      last.valid  = term.valid;
      dirty       = true;
      initialized = false;
    }

    const Grid grid = grid_from_frame(core::Size{width, height});

    if (!initialized && grid.w >= 5 && grid.h >= 5) {
      reset_game(state, grid, rng);
      initialized = true;
      dirty       = true;
    }

    const auto now = std::chrono::steady_clock::now();
    if (initialized && now >= next_tick) {
      auto tick_ms = state.tick_ms;
      if (state.dir == Dir::Up || state.dir == Dir::Down) {
        tick_ms = tick_ms + 6;
      }
      next_tick = now + std::chrono::milliseconds(tick_ms);
      if (state.alive && !state.paused) {
        const auto next = step(state.snake.front(), state.dir);
        if (next.x < 0 || next.y < 0 || next.x >= grid.w || next.y >= grid.h ||
            contains(state.snake, next) || contains(state.obstacles, next)) {
          state.alive = false;
        }
        else {
          state.snake.push_front(next);
          if (next.x == state.food.x && next.y == state.food.y) {
            state.score += 1;
            state.apples += 1;
            state.tick_ms = next_tick_ms(90, state.apples);
            state.food =
                random_empty_cell(rng, grid, state.snake, state.obstacles);
          }
          else {
            state.snake.pop_back();
          }
        }
        dirty = true;
      }
    }

    if (auto ev = input.poll(); !std::holds_alternative<std::monostate>(ev)) {
      if (auto key = std::get_if<core::KeyEvent>(&ev)) {
        if (key->code == core::KeyCode::Esc) {
          break;
        }
        if (key->code == core::KeyCode::Char) {
          const char32_t ch = key->ch;
          if (ch == U'q' || ch == U'Q') {
            break;
          }
          if (ch == U'p' || ch == U'P') {
            state.paused = !state.paused;
            dirty        = true;
          }
          if (ch == U'r' || ch == U'R') {
            if (grid.w >= 5 && grid.h >= 5) {
              reset_game(state, grid, rng);
              initialized = true;
              dirty       = true;
            }
          }
          if (ch == U'w' || ch == U'W') {
            if (!is_opposite(state.dir, Dir::Up)) {
              state.dir = Dir::Up;
              dirty     = true;
            }
          }
          if (ch == U's' || ch == U'S') {
            if (!is_opposite(state.dir, Dir::Down)) {
              state.dir = Dir::Down;
              dirty     = true;
            }
          }
          if (ch == U'a' || ch == U'A') {
            if (!is_opposite(state.dir, Dir::Left)) {
              state.dir = Dir::Left;
              dirty     = true;
            }
          }
          if (ch == U'd' || ch == U'D') {
            if (!is_opposite(state.dir, Dir::Right)) {
              state.dir = Dir::Right;
              dirty     = true;
            }
          }
        }
        if (key->code == core::KeyCode::Up &&
            !is_opposite(state.dir, Dir::Up)) {
          state.dir = Dir::Up;
          dirty     = true;
        }
        if (key->code == core::KeyCode::Down &&
            !is_opposite(state.dir, Dir::Down)) {
          state.dir = Dir::Down;
          dirty     = true;
        }
        if (key->code == core::KeyCode::Left &&
            !is_opposite(state.dir, Dir::Left)) {
          state.dir = Dir::Left;
          dirty     = true;
        }
        if (key->code == core::KeyCode::Right &&
            !is_opposite(state.dir, Dir::Right)) {
          state.dir = Dir::Right;
          dirty     = true;
        }
      }
    }

    if (!dirty) {
      std::this_thread::sleep_for(1ms);
      continue;
    }

    view::Frame frame{core::Size{width, height}};
    frame.fill(core::Cell(U' '));

    if (width < 20 || height < 8) {
      view::draw_text(
          frame, {0, 0}, "Terminal too small for Snake.", core::Cell(U'!'));
      app.render(frame);
      dirty = false;
      continue;
    }

    if (grid.w < 5 || grid.h < 5) {
      view::draw_text(
          frame, {0, 0}, "Terminal too small for Snake.", core::Cell(U'!'));
      app.render(frame);
      dirty       = false;
      initialized = false;
      continue;
    }

    ScoreBoardView score_view(&state);
    SnakeView      snake_view(&state);

    view::PanelView game_panel(&snake_view);
    game_panel.set_fill(core::Cell(U' '));
    game_panel.set_border(core::Cell(U'#'));
    game_panel.set_padding(kGamePadding);
    game_panel.set_draw_fill(true);
    game_panel.set_draw_border(true);

    auto layout = view::VStack(
        {
            view::fixed(&score_view, kStatusHeight),
            view::flex(&game_panel),
        },
        0);

    layout.render(frame, frame.bounds());

    app.render(frame);
    dirty = false;
  }

  return 0;
}

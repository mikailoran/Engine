#pragma once

// Standard headers only: components include this

/** @brief A position in window pixels, origin top-left. */
struct ScreenPosition {
  float x{0.0F};
  float y{0.0F};
};

/** @brief Window size in pixels. */
struct ScreenSize {
  float width{0.0F};
  float height{0.0F};
};

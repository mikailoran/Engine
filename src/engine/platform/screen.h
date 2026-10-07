#pragma once

// Standard headers only: components include this

#include <cstdint>

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

/** @brief Backbuffer size in whole pixels. */
struct PixelSize {
  std::uint32_t width{0};
  std::uint32_t height{0};

  friend auto operator==(const PixelSize&, const PixelSize&) -> bool = default;
};

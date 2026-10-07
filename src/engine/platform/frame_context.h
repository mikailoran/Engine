#pragma once

#include <cstdint>

#include "engine/platform/input.h"

namespace engine {

/**
 * @brief Generic info used per-frame for rendering
 */
struct FrameContext {
  // Backbuffer width in pixels.
  uint32_t width{1280};

  // Backbuffer height in pixels.
  uint32_t height{720};

  // Seconds elapsed since the previous frame.
  float dt{0.0F};

  // Seconds elapsed since startup.
  float time{0.0F};

  // This frame's mouse input; default-constructed when the host has none.
  Input input{};
};

}  // namespace engine

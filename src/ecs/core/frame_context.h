#pragma once

#include <cstdint>

// Forward declared rather than including entry/entry.h, to keep the examples
// framework out of ecs/core. Systems that dereference the pointer include it
// themselves.
namespace entry {
struct MouseState;
} // namespace entry

/**
 * @brief Per-frame inputs that are not per-entity state.
 *
 * Passed by const reference to every system's Update. Exists because systems
 * need things the ECS cannot supply from components alone: the backbuffer size,
 * elapsed time, and raw input.
 */
struct FrameContext {
  /// Backbuffer width in pixels; entry writes this back on resize.
  uint32_t width{1280};

  /// Backbuffer height in pixels.
  uint32_t height{720};

  /// Seconds elapsed since the previous frame.
  float dt{0.0F};

  /// Seconds elapsed since startup. Feeds the u_time shader uniform.
  float time{0.0F};

  /// Current mouse position and button state. Never null in practice; owned by
  /// the caller and only valid for the duration of the Update call.
  const entry::MouseState *mouse{nullptr};
};

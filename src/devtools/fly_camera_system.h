#pragma once

#include <optional>

#include "engine/ecs/core/types.h"
#include "engine/math/rotation.h"
#include "engine/platform/screen.h"

class Ecs;
struct FrameContext;

/**
 * @brief Flies a camera entity from mouse and keyboard input.
 *
 * Right-drag turns, W/A/S/D move, Q/E go down and up, and the wheel moves
 * along the view. Owns the camera's rotation: it is rewritten every frame
 * from this system's yaw and pitch, which start level along +Z.
 */
class FlyCameraSystem {
 public:
  /**
   * @brief Turns and moves the controlled camera's Transform. Does nothing
   * until SetControlledCamera.
   *
   * @param ecs World to write the Transform through.
   * @param ctx Per-frame inputs.
   */
  void Update(Ecs& ecs, const FrameContext& ctx);

  /** @brief Nominates the entity to fly. It must carry a Transform. */
  void SetControlledCamera(Entity camera);

 private:
  // Entity being flown; empty until SetControlledCamera.
  std::optional<Entity> camera_;

  // Look direction, applied to the camera's rotation every frame.
  YawPitch angles_{};

  // Cursor position last frame, to turn mouse motion into turning.
  ScreenPosition last_mouse_{};
};

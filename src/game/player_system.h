#pragma once

#include <optional>

#include "engine/ecs/core/types.h"
#include "engine/math/rotation.h"

namespace engine {
class Ecs;
struct FrameContext;
}  // namespace engine

namespace game {

/**
 * @brief Drives the Player's character from input and places the first-person
 * camera at its eyes.
 *
 * W/A/S/D walk relative to where the player faces, Shift sprints, Space jumps
 * from the ground, and mouse motion looks around. Owns the look direction.
 */
class PlayerSystem {
 public:
  /** @brief Nominates the camera PlaceCamera moves to the player's eyes. */
  void SetViewCamera(engine::Entity camera);

  /**
   * @brief Turns this frame's input into the player's velocity and facing.
   * Run before physics.
   * @param has_control False stops the player where it stands.
   */
  void Control(engine::Ecs& ecs, const engine::FrameContext& ctx,
               bool has_control);

  /** @brief Moves the view camera to the player's eyes. Run after physics. */
  void PlaceCamera(engine::Ecs& ecs) const;

 private:
  /// Turn per pixel of mouse motion, in radians.
  static constexpr float kLookRadiansPerPixel = 0.002F;

  /// Steepest look up or down, short of straight, in radians.
  static constexpr float kMaxPitch = 1.55F;

  engine::YawPitch angles_{};
  std::optional<engine::Entity> camera_;
};

}  // namespace game

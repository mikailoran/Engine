#pragma once

#include <optional>

#include "ecs/core/types.h"

class Ecs;
struct FrameContext;

/**
 * @brief Draws a ground grid, origin axes and every Collider's wireframe.
 *
 * Owns bgfx's debugdraw context, a process-wide global, so at most one
 * instance may exist. Draws to view 1, over the scene and against its depth.
 */
class DebugDrawSystem {
 public:
  /** @brief Creates the debugdraw context. Requires bgfx::init. */
  DebugDrawSystem();

  /** @brief Destroys the debugdraw context. Must run before bgfx::shutdown. */
  ~DebugDrawSystem();

  // Owns a global: copying or moving would destroy it twice
  DebugDrawSystem(const DebugDrawSystem&) = delete;
  auto operator=(const DebugDrawSystem&) -> DebugDrawSystem& = delete;
  DebugDrawSystem(DebugDrawSystem&&) = delete;
  auto operator=(DebugDrawSystem&&) -> DebugDrawSystem& = delete;

  /**
   * @brief Submits this frame's debug geometry. Must precede bgfx::frame().
   *
   * Draws nothing until a camera is nominated.
   */
  void Update(Ecs& ecs, const FrameContext& ctx);

  /**
   * @brief Nominates the entity supplying view and projection.
   * @param camera Entity carrying a Camera.
   */
  void SetCamera(Entity camera);

 private:
  // Entity supplying view and projection; empty until SetCamera.
  std::optional<Entity> camera_;
};

#pragma once

#include <optional>
#include <vector>

#include "engine/ecs/core/types.h"

class AssetRegistry;
class Ecs;
struct FrameContext;

/**
 * @brief Draws a ground grid, origin axes, every Collider's wireframe,
 * every RigidBody's velocity arrow, and boxes around highlighted entities.
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
   * Draws nothing until a camera is nominated. Clears this frame's highlight
   * requests either way.
   *
   * @param assets Registry to resolve highlighted entities' mesh bounds.
   */
  void Update(Ecs& ecs, const AssetRegistry& assets, const FrameContext& ctx);

  /**
   * @brief Requests a wire box around @p entity's mesh for this frame only.
   *
   * Entities without a Transform and a Renderable are skipped.
   */
  void Highlight(Entity entity);

  /**
   * @brief Nominates the entity supplying view and projection.
   * @param camera Entity carrying a Camera.
   */
  void SetActiveCamera(Entity camera);

 private:
  // Entity supplying view and projection; empty until SetActiveCamera.
  std::optional<Entity> active_camera_;

  // This frame's Highlight requests, cleared by Update.
  std::vector<Entity> highlights_;
};

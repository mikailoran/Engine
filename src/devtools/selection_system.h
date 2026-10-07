#pragma once

#include <optional>

#include "engine/ecs/core/types.h"

class Ecs;
class AssetRegistry;
struct FrameContext;

/**
 * @brief Selects the entity under the cursor on a left click.
 *
 * Picks from the nominated camera through the cursor (see Pick). The entity
 * hit gets Selected; a miss clears the selection. Single selection only.
 */
class SelectionSystem {
 public:
  /**
   * @brief Picks on a left-button press and updates Selected.
   *
   * Reads the camera's pose, so must run after CameraControl.
   *
   * @param ecs World to pick from and write Selected through.
   * @param assets Registry to resolve mesh bounds through.
   * @param ctx Per-frame inputs; reads the mouse and window size.
   * @param mouse_over_ui True when the UI owns the cursor; clicks are ignored.
   */
  void Update(Ecs& ecs, const AssetRegistry& assets, const FrameContext& ctx,
              bool mouse_over_ui);

  /**
   * @brief Nominates the entity to cast rays from. It must carry a Camera.
   */
  void SetCamera(Entity camera);

 private:
  // Entity supplying view and projection; empty until SetCamera.
  std::optional<Entity> camera_;
};

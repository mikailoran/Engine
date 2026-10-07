#pragma once

#include <optional>

#include "engine/ecs/core/types.h"

class Ecs;
struct FrameContext;

namespace entry {
struct MouseState;
}  // namespace entry

/**
 * @brief Controls camera entities from user input.
 *
 * Currently using examples-common free-look camera. The system copies its
 * resulting pose into the components. Owns that camera, a process-wide
 * global, so at most one instance may exist.
 */
class CameraControl {
 public:
  /** @brief Creates the underlying camera and sets its initial pose. */
  CameraControl();

  /** @brief Destroys the underlying camera. */
  ~CameraControl();

  // Owns a global: copying or moving would destroy it twice
  CameraControl(const CameraControl&) = delete;
  auto operator=(const CameraControl&) -> CameraControl& = delete;
  CameraControl(CameraControl&&) = delete;
  auto operator=(CameraControl&&) -> CameraControl& = delete;

  /**
   * @brief Gathers input and writes the resulting pose, view and projection
   * into the nominated camera entity's Transform and Camera. Does nothing
   * until SetCamera.
   *
   * @param ecs World to write components through.
   * @param ctx Per-frame inputs.
   * @param mouse entry's raw mouse state, which the examples' camera reads.
   */
  void Update(Ecs& ecs, const FrameContext& ctx,
              const entry::MouseState& mouse);

  /**
   * @brief Nominates the entity to drive. It must carry a Transform and a
   * Camera.
   */
  void SetCamera(Entity camera);

 private:
  // Entity receiving the pose; empty until SetCamera.
  std::optional<Entity> camera_;
};

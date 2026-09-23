#pragma once

#include "../core/system.h"

class Ecs;
struct FrameContext;

/**
 * @brief Controls camera entities from user input.
 *
 * Currently using examples-common free-look camera. The system copies its
 * resulting pose into the components.
 */
class CameraControl : public System {
public:
  /**
   * @brief Creates the underlying camera and sets its initial pose.
   */
  void Init();

  /**
   * @brief Gathers input and writes the resulting pose into tracked camera
   * entity's Transform and Camera.
   *
   * @param ecs World to write components through.
   * @param ctx Per-frame inputs.
   */
  void Update(Ecs &ecs, const FrameContext &ctx);

  // Destroys the underlying camera.
  void Shutdown();

private:
};

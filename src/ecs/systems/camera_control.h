#pragma once

#include "../core/system.h"

class Ecs;
struct FrameContext;

/**
 * @brief Drives camera entities from user input. Matches {Transform, Camera}.
 *
 * Currently a bridge over the examples-common free-look camera: that camera
 * owns the WASD/QE and mouse-look handling (cameraCreate registers the input
 * bindings), and this system copies its resulting pose into the components so
 * that RenderSystem can read placement from the ECS rather than from a global.
 *
 * Replacing examples-common with entry's input API directly is a later,
 * separate pass; keeping the bridge here means that swap touches only this
 * file.
 */
class CameraControl : public System {
public:
  /**
   * @brief Creates the underlying camera and sets its initial pose.
   *
   * Also registers the examples-common input bindings as a side effect, so it
   * must run before the first Update.
   */
  void Init();

  /**
   * @brief Pumps input and writes the resulting pose into every tracked
   *        camera entity's Transform and Camera.
   *
   * @param ecs World to write components through.
   * @param ctx Per-frame inputs; uses dt and mouse.
   */
  void Update(Ecs &ecs, const FrameContext &ctx);

  /// Destroys the underlying camera. Safe to call before bgfx::shutdown.
  void Shutdown();

private:
};

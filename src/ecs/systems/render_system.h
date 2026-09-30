#pragma once

#include "../core/system.h"

#include <bgfx/bgfx.h>

class Ecs;
class AssetRegistry;
struct FrameContext;

/**
 * @brief Draws every entity carrying {Transform, Renderable}.
 *
 * Owns view 0 and the resources shared across entities (shader program and
 * uniforms)
 */
class RenderSystem : public System {
public:
  /**
   * @brief Creates the shared GPU resources.
   *
   * Requires bgfx::init to have completed, and the asset root to be set via
   * entry::setCurrentDir, since loadProgram resolves its paths against it.
   *
   * @param assets Registry to resolve mesh handles through. Must outlive this
   * system.
   */
  void Init(const AssetRegistry &assets);

  /**
   * @brief Submits one frame.
   *
   * @param ecs World to read Transform and Renderable from.
   * @param ctx Per-frame inputs.
   */
  void Update(Ecs &ecs, const FrameContext &ctx);

  /**
   * @brief Destroys every GPU resource this system owns.
   *
   * Must run before bgfx::shutdown.
   */
  void Shutdown();

  /**
   * @brief Nominates the entity supplying view and projection.
   *
   * The entity must carry both a Transform  and a Camera.
   * Until this is called the view is identity, which leaves the scene drawn
   * from the world origin.
   *
   * @param camera Entity to read the camera from.
   */
  void SetCamera(Entity camera);

private:
  // TODO: figure out how to reinforce class invariants
  const AssetRegistry *assets_{nullptr};

  // Used when a Renderable leaves its own program handle invalid.
  bgfx::ProgramHandle default_program_{bgfx::kInvalidHandle};

  bgfx::UniformHandle u_time_{bgfx::kInvalidHandle};

  // Per-draw surface color, set from Renderable::color before each submit.
  bgfx::UniformHandle u_color_{bgfx::kInvalidHandle};

  // Per-frame camera world position.
  bgfx::UniformHandle u_eye_pos_{bgfx::kInvalidHandle};

  // Entity supplying view and projection; only read when has_camera_ is set.
  Entity camera_{0};
  bool has_camera_{false};
};

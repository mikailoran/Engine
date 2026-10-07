#pragma once

#include <bgfx/bgfx.h>

#include <optional>

#include "engine/ecs/core/types.h"
#include "engine/resource/unique_handle.h"

namespace engine {

class Ecs;
class AssetRegistry;
struct FrameContext;

/**
 * @brief Draws every entity carrying {Transform, Renderable}.
 *
 * Owns view 0 and the resources shared across entities (shader program,
 * uniforms and the fallback white texture). Releases them on destruction,
 * which must precede bgfx::shutdown.
 */
class RenderSystem {
 public:
  /**
   * @brief Creates the shared GPU resources and sets view 0's clear.
   *
   * Requires bgfx::init to have completed, and the asset root to be set via
   * SetAssetRoot, since loadProgram resolves its paths against it.
   *
   * @throws std::runtime_error If the default program fails to link.
   */
  RenderSystem();

  /**
   * @brief Submits one frame.
   *
   * @param ecs World to read Transform and Renderable from.
   * @param assets Registry to resolve mesh and texture handles through.
   * @param ctx Per-frame inputs.
   */
  void Update(Ecs& ecs, const AssetRegistry& assets, const FrameContext& ctx);

  /**
   * @brief Nominates the entity supplying view and projection.
   *
   * The entity must carry both a Transform and a Camera.
   * Until this is called the scene is drawn from the world origin with a
   * default Camera.
   *
   * @param camera Entity to read the camera from.
   */
  void SetActiveCamera(Entity camera);

 private:
  // Used when a Renderable leaves its own program handle invalid.
  UniqueHandle<bgfx::ProgramHandle> default_program_;

  UniqueHandle<bgfx::UniformHandle> u_time_;

  // Per-draw surface color, set from Renderable::color before each submit.
  UniqueHandle<bgfx::UniformHandle> u_color_;

  // Per-frame camera world position.
  UniqueHandle<bgfx::UniformHandle> u_eye_pos_;

  // Albedo sampler, stage 0.
  UniqueHandle<bgfx::UniformHandle> s_albedo_;

  // Per-draw texture tiling, set from Renderable::texture_scale.
  UniqueHandle<bgfx::UniformHandle> u_tex_params_;

  // 1x1 white, bound for untextured entities so they keep their plain color.
  UniqueHandle<bgfx::TextureHandle> default_texture_;

  // Entity supplying view and projection; empty until SetActiveCamera.
  std::optional<Entity> active_camera_;
};

}  // namespace engine

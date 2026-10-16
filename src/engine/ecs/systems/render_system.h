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
 * @brief Draws every entity carrying {Transform, Renderable}; a skinned mesh
 * whose entity has a SkinPose is drawn posed, otherwise in bind pose.
 *
 * Owns kSceneView and the resources shared across entities (shader program,
 * uniforms and the fallback white texture). Releases them on destruction,
 * which must precede bgfx::shutdown.
 */
class RenderSystem {
 public:
  /**
   * @brief Creates the shared GPU resources and sets kSceneView's clear.
   *
   * Requires bgfx::init to have completed, and the asset root to be set via
   * SetAssetRoot, since loadProgram resolves its paths against it.
   *
   * @throws std::runtime_error If a default program fails to link.
   */
  RenderSystem();

  /**
   * @brief Submits one frame.
   *
   * @param ecs World to read Transform and Renderable from.
   * @param assets Registry to resolve mesh handles through.
   * @param ctx Per-frame inputs.
   * @param shadow_map The sun's depth map, from ShadowSystem::Map.
   */
  void Update(Ecs& ecs, const AssetRegistry& assets, const FrameContext& ctx,
              bgfx::TextureHandle shadow_map);

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

  // The default program's skinned twin, for entities with a SkinPose.
  UniqueHandle<bgfx::ProgramHandle> skinned_program_;

  // Per-draw skinning palette, kMaxJoints Mat4s.
  UniqueHandle<bgfx::UniformHandle> u_bones_;

  UniqueHandle<bgfx::UniformHandle> u_time_;

  // Per-draw surface color: the material's base color times the entity's tint.
  UniqueHandle<bgfx::UniformHandle> u_color_;

  // Per-draw material factors: x metallic, y roughness.
  UniqueHandle<bgfx::UniformHandle> u_material_;

  // Per-draw linear RGB emission, multiplied with the emissive texture.
  UniqueHandle<bgfx::UniformHandle> u_emissive_;

  // Per-frame camera world position.
  UniqueHandle<bgfx::UniformHandle> u_eye_pos_;

  // Base color sampler, stage 0.
  UniqueHandle<bgfx::UniformHandle> s_albedo_;

  // Shadow map sampler, stage 1.
  UniqueHandle<bgfx::UniformHandle> s_shadow_map_;

  // Emissive color sampler, stage 2.
  UniqueHandle<bgfx::UniformHandle> s_emissive_;

  // 1x1 white, bound for missing textures so their factor applies unchanged.
  UniqueHandle<bgfx::TextureHandle> default_texture_;

  // Entity supplying view and projection; empty until SetActiveCamera.
  std::optional<Entity> active_camera_;
};

}  // namespace engine

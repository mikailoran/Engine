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
 * @brief Renders the DirectionalLight's shadow map from every Renderable.
 *
 * Draws depth into kShadowView, which bgfx renders before the scene, over the
 * active camera's nearest few tens of metres. Publishes u_shadowMtx (world to
 * shadow map UV and depth) and u_shadowParams as frame uniforms, so it must
 * run before RenderSystem submits the frame. Releases its GPU resources on
 * destruction, which must precede bgfx::shutdown.
 */
class ShadowSystem {
 public:
  /**
   * @brief Creates the shadow map, its framebuffer, program and uniforms.
   *
   * Requires bgfx::init and the asset root to be set, since loadProgram
   * resolves its paths against it.
   *
   * @throws std::runtime_error If the program fails to link or the backend
   *         cannot compare depth when sampling.
   */
  ShadowSystem();

  /**
   * @brief Draws this frame's shadow map and sets the shadow uniforms.
   *
   * Uses a default-constructed DirectionalLight when no entity has one, and a
   * default Camera at the origin until SetActiveCamera.
   */
  void Update(Ecs& ecs, const AssetRegistry& assets, const FrameContext& ctx);

  /**
   * @brief Nominates the camera whose view the map covers. The entity must
   * carry a Transform and a Camera.
   */
  void SetActiveCamera(Entity camera);

  /**
   * @brief The depth map, for the scene pass to sample. Owned by this system;
   * created with compare-LEQUAL sampling and clamped addressing.
   */
  [[nodiscard]] auto Map() const -> bgfx::TextureHandle;

 private:
  UniqueHandle<bgfx::ProgramHandle> program_;

  // Owns the depth texture it renders into.
  UniqueHandle<bgfx::FrameBufferHandle> framebuffer_;

  // World to shadow map UV (xy) and depth (z).
  UniqueHandle<bgfx::UniformHandle> u_shadow_mtx_;

  // x: normal offset in metres, y: depth bias, z: one texel in UV.
  UniqueHandle<bgfx::UniformHandle> u_shadow_params_;

  // Entity supplying the covered view; empty until SetActiveCamera.
  std::optional<Entity> active_camera_;
};

}  // namespace engine

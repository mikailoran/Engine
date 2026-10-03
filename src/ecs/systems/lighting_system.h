#pragma once

#include <bgfx/bgfx.h>

class Ecs;
struct FrameContext;

/**
 * @brief Publishes the scene's DirectionalLight as per-frame shader uniforms.
 *
 * Reads the entity carrying a DirectionalLight; at most one is expected. Only
 * sets uniforms, so it must run before RenderSystem submits the frame.
 */
class LightingSystem {
 public:
  LightingSystem() = default;
  LightingSystem(const LightingSystem&) = delete;
  auto operator=(const LightingSystem&) -> LightingSystem& = delete;

  /** @brief Creates the light uniforms. Requires bgfx::init to have completed.
   */
  void Init();

  /**
   * @brief Sets the sun and ambient uniforms for this frame.
   *
   * Falls back to a default-constructed DirectionalLight when no entity has
   * one.
   */
  void Update(Ecs& ecs, const FrameContext& ctx);

  /** @brief Destroys the light uniforms. Must run before bgfx::shutdown. */
  void Shutdown();

 private:
  bgfx::UniformHandle u_light_dir_{bgfx::kInvalidHandle};
  bgfx::UniformHandle u_light_color_{bgfx::kInvalidHandle};
  bgfx::UniformHandle u_sky_color_{bgfx::kInvalidHandle};
  bgfx::UniformHandle u_ground_color_{bgfx::kInvalidHandle};
};

#pragma once

#include <bgfx/bgfx.h>

#include "engine/resource/unique_handle.h"

namespace engine {

class Ecs;
struct FrameContext;

/**
 * @brief Publishes the scene's DirectionalLight as per-frame shader uniforms.
 *
 * Reads the entity carrying a DirectionalLight; at most one is expected. Only
 * sets uniforms, so it must run before RenderSystem submits the frame.
 * Releases the uniforms on destruction, which must precede bgfx::shutdown.
 */
class LightingSystem {
 public:
  /** @brief Creates the light uniforms. Requires bgfx::init to have completed.
   */
  LightingSystem();

  /**
   * @brief Sets the sun and ambient uniforms for this frame.
   *
   * Falls back to a default-constructed DirectionalLight when no entity has
   * one.
   */
  void Update(Ecs& ecs, const FrameContext& ctx);

 private:
  UniqueHandle<bgfx::UniformHandle> u_light_dir_;
  UniqueHandle<bgfx::UniformHandle> u_light_color_;
  UniqueHandle<bgfx::UniformHandle> u_sky_color_;
  UniqueHandle<bgfx::UniformHandle> u_ground_color_;
};

}  // namespace engine

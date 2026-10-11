#pragma once

#include <bgfx/bgfx.h>

#include "engine/resource/unique_handle.h"

namespace engine {

class Ecs;

/**
 * @brief Draws the scene's Sky behind all geometry in view 0.
 *
 * One fullscreen triangle at the far plane, depth-tested so only pixels no
 * geometry covers show it. The sun is placed by LightingSystem's frame
 * uniforms, so both must run before RenderSystem submits the frame. Releases
 * its GPU resources on destruction, which must precede bgfx::shutdown.
 */
class SkySystem {
 public:
  /**
   * @brief Loads the sky program and creates its uniforms and triangle.
   *
   * Requires bgfx::init to have completed and the asset root to be set, since
   * loadProgram resolves its paths against it.
   *
   * @throws std::runtime_error If the sky program fails to link.
   */
  SkySystem();

  /**
   * @brief Submits this frame's sky to view 0.
   *
   * Draws a default-constructed Sky when no entity has one.
   */
  void Update(Ecs& ecs);

 private:
  UniqueHandle<bgfx::ProgramHandle> program_;

  // Clip-space triangle whose clipped area is the whole viewport.
  UniqueHandle<bgfx::VertexBufferHandle> triangle_;

  UniqueHandle<bgfx::UniformHandle> u_zenith_;
  UniqueHandle<bgfx::UniformHandle> u_horizon_;
  UniqueHandle<bgfx::UniformHandle> u_ground_;

  // x: cos of sun radius, y: glow sharpness, z: gradient falloff, w: glow
  // strength.
  UniqueHandle<bgfx::UniformHandle> u_params_;
};

}  // namespace engine

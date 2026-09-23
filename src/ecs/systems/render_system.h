#pragma once

#include "../core/system.h"

#include <bgfx/bgfx.h>

class Ecs;
struct FrameContext;

/**
 * @brief Draws every entity carrying {Transform, Renderable}, plus the static
 *        floor.
 *
 * Owns view 0 and the resources shared across entities: the default shader
 * program, the u_time uniform, and the floor's geometry. The floor is not an
 * entity — it has no Transform and never moves — so it lives here rather than
 * in the ECS.
 *
 * This system is the sole submitter: its Update ends with bgfx::frame().
 */
class RenderSystem : public System {
public:
  /**
   * @brief Creates the shared GPU resources.
   *
   * Requires bgfx::init to have completed, and the working directory to be set,
   * since loadProgram resolves its paths relative to it.
   */
  void Init();

  /**
   * @brief Submits one frame: view setup, every tracked entity, then the floor.
   *
   * @param ecs World to read Transform and Renderable from.
   * @param ctx Per-frame inputs; supplies viewport size and elapsed time.
   */
  void Update(Ecs &ecs, const FrameContext &ctx);

  /**
   * @brief Destroys every GPU resource this system owns, including the mesh
   *        held by each tracked entity's Renderable.
   *
   * Must run before bgfx::shutdown.
   *
   * @param ecs World to walk for owned meshes.
   */
  void Shutdown(Ecs &ecs);

  /**
   * @brief Nominates the entity supplying view and projection.
   *
   * The entity must carry both a Transform (eye position) and a Camera
   * (target, up, and the projection parameters). Until this is called the
   * view is identity, which leaves the scene drawn from the world origin.
   *
   * @param camera Entity to read the camera from.
   */
  void SetCamera(Entity camera);

private:
  /// Used when a Renderable leaves its own program handle invalid.
  bgfx::ProgramHandle default_program_{bgfx::kInvalidHandle};

  bgfx::UniformHandle u_time_{bgfx::kInvalidHandle};

  bgfx::VertexBufferHandle floor_vbh_{bgfx::kInvalidHandle};
  bgfx::IndexBufferHandle floor_ibh_{bgfx::kInvalidHandle};

  /// Entity supplying view and projection; only read when has_camera_ is set.
  Entity camera_{0};
  bool has_camera_{false};
};

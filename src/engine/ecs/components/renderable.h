#pragma once

#include <bgfx/bgfx.h>

#include <array>
#include <cstdint>

#include "engine/resource/mesh_handle.h"

namespace engine {

/**
 * @brief Draws its entity's mesh at the entity's Transform, one draw per
 * submesh. Read by RenderSystem, picking and debug draw.
 */
struct Renderable {
  /// The mesh to draw, owned by AssetRegistry and shared with other entities.
  MeshHandle mesh_handle;

  // Shader program. An invalid handle uses RenderSystem's default.
  // Not owned by the component.
  bgfx::ProgramHandle program{bgfx::kInvalidHandle};

  // Render state. Culls clockwise: front faces are counter-clockwise on screen
  std::uint64_t state{BGFX_STATE_DEFAULT};

  // Which bgfx view to submit into.
  bgfx::ViewId view{0};

  /// Linear RGBA tint. White leaves the mesh's own colors unchanged.
  std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
};

}  // namespace engine

#pragma once

#include <bgfx/bgfx.h>

#include <array>
#include <cstdint>

#include "engine/resource/mesh_handle.h"
#include "engine/resource/texture_handle.h"

namespace engine {

/**
 * @brief Everything needed to issue one draw call for an entity.
 */
struct Renderable {
  /// Non owning handle for the Rendrable's mesh. Used with AssetRegistry
  MeshHandle mesh_handle{};

  // Shader program. An invalid handle uses RenderSystem's default.
  // Not owned by the component.
  bgfx::ProgramHandle program{bgfx::kInvalidHandle};

  // Render state. Culls clockwise: front faces are counter-clockwise on screen
  std::uint64_t state{BGFX_STATE_DEFAULT};

  // Which bgfx view to submit into.
  bgfx::ViewId view{0};

  std::array<float, 4> color{0.8F, 0.8F, 0.8F, 1.0F};

  /// Non owning albedo texture, tinted by color. Invalid means untextured.
  TextureHandle texture{};

  /// World units covered by one repeat of the texture. Must be positive.
  float texture_scale{1.0F};
};

}  // namespace engine

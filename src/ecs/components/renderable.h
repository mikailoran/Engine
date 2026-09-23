#pragma once

#include "../../assets/mesh_handle.h"

#include <bgfx/bgfx.h>
#include <cstdint>

/**
 * @brief Everything needed to issue one draw call for an entity.
 */
struct Renderable {
  /// Non owning handle for the Rendrable's mesh. Used with AssetRegistry
  MeshHandle mesh_handle{};

  // Shader program. An invalid handle uses RenderSystem's default.
  // Not owned by the component.
  bgfx::ProgramHandle program{bgfx::kInvalidHandle};

  // Render state
  std::uint64_t state{BGFX_STATE_MASK};

  // Which bgfx view to submit into.
  bgfx::ViewId view{0};
};

#pragma once

#include <bgfx/bgfx.h>
#include <cstdint>

// Defined in examples/common/bgfx_utils.h.
struct Mesh;

/**
 * @brief Everything needed to issue one draw call for an entity.
 */
struct Renderable {
  // Owned by this component. RenderSystem::Shutdown calls meshUnload on it.
  // Never null for a registered entity.
  // TODO: use unique_ptr
  Mesh *mesh{nullptr};

  // Shader program. An invalid handle uses RenderSystem's default.
  // Not owned by the component.
  bgfx::ProgramHandle program{bgfx::kInvalidHandle};

  // Render state
  std::uint64_t state{BGFX_STATE_MASK};

  // Which bgfx view to submit into.
  bgfx::ViewId view{0};
};

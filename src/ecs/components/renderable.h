#pragma once

#include <bgfx/bgfx.h>
#include <cstdint>

// Defined in examples/common/bgfx_utils.h. Only ever held by pointer here, so
// the declaration is enough and keeps that header out of every translation
// unit that touches components.
struct Mesh;

/**
 * @brief Everything needed to issue one draw call for an entity.
 *
 * Paired with a Transform, which supplies the model matrix. RenderSystem
 * matches on {Transform, Renderable}.
 */
struct Renderable {
  /// Geometry to draw. Owned by this component: RenderSystem::Shutdown calls
  /// meshUnload on it. Never null for a registered entity.
  // TODO: use unique_ptr
  Mesh *mesh{nullptr};

  /// Shader program. An invalid handle means "use RenderSystem's default".
  /// Never owned here — one program is currently shared by every entity, so
  /// per-component ownership would double-destroy at shutdown.
  bgfx::ProgramHandle program{bgfx::kInvalidHandle};

  /// Render state passed through to meshSubmit. BGFX_STATE_MASK would instead
  /// defer to the state baked into each of the mesh's own groups.
  std::uint64_t state{BGFX_STATE_MASK};

  /// Which bgfx view to submit into.
  bgfx::ViewId view{0};
};

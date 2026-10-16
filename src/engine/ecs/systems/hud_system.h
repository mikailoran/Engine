#pragma once

#include <bgfx/bgfx.h>

#include <optional>

#include "engine/hud/crosshair.h"
#include "engine/resource/unique_handle.h"

namespace engine {

struct FrameContext;

/**
 * @brief Draws screen-space overlays over the finished scene in kHudView.
 *
 * Immediate mode: requests last one frame, and Update clears them. Releases
 * its GPU resources on destruction, which must precede bgfx::shutdown.
 */
class HudSystem {
 public:
  /**
   * @brief Loads the HUD program and creates its uniforms and quad.
   *
   * Requires bgfx::init to have completed and the asset root to be set.
   *
   * @throws std::runtime_error If the HUD program fails to link.
   */
  HudSystem();

  /**
   * @brief Requests a crosshair at the screen centre this frame. A later
   * request in the same frame replaces it.
   */
  void DrawCrosshair(const CrosshairStyle& style);

  /**
   * @brief Submits this frame's requests to kHudView if @p visible, then
   * clears them either way. Must precede RenderSystem's bgfx::frame.
   */
  void Update(const FrameContext& ctx, bool visible);

 private:
  UniqueHandle<bgfx::ProgramHandle> program_;

  // Unit quad, corners at +-1, drawn as a triangle strip.
  UniqueHandle<bgfx::VertexBufferHandle> quad_;

  // x: gap, y: arm length, z: thickness, w: outline, all in px.
  UniqueHandle<bgfx::UniformHandle> u_shape_;
  // x: dot radius, y: half the quad's size, in px.
  UniqueHandle<bgfx::UniformHandle> u_extra_;
  UniqueHandle<bgfx::UniformHandle> u_color_;
  UniqueHandle<bgfx::UniformHandle> u_outline_;

  // This frame's crosshair request, if any.
  std::optional<CrosshairStyle> crosshair_;
};

}  // namespace engine

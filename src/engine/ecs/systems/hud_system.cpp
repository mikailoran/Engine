#include "engine/ecs/systems/hud_system.h"

#include <bgfx/bgfx.h>
#include <bgfx/defines.h>
#include <bgfx_utils.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>

#include "engine/hud/crosshair.h"
#include "engine/platform/frame_context.h"
#include "engine/render_views.h"
#include "engine/resource/unique_handle.h"

namespace engine {

namespace {

// bgfx's macro repeats its operands for the RGB and alpha factors
// NOLINTNEXTLINE(misc-redundant-expression)
constexpr std::uint64_t kAlphaBlend = BGFX_STATE_BLEND_ALPHA;

// Blended over the frame, ignoring its depth
constexpr std::uint64_t kHudState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                                    kAlphaBlend | BGFX_STATE_PT_TRISTRIP;

/// Pixels past the shape's edge, so its antialiased rim isn't clipped.
constexpr float kAaMargin = 1.0F;

/** @brief Uploads a unit quad, corners at +-1, as a triangle strip. */
auto CreateUnitQuad() -> UniqueHandle<bgfx::VertexBufferHandle> {
  constexpr std::array<float, 12> kVertices{
      -1.0F, -1.0F, 0.0F,  //
      1.0F,  -1.0F, 0.0F,  //
      -1.0F, 1.0F,  0.0F,  //
      1.0F,  1.0F,  0.0F,
  };
  bgfx::VertexLayout layout;
  layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
  return UniqueHandle(bgfx::createVertexBuffer(
      bgfx::copy(kVertices.data(), sizeof(kVertices)), layout));
}

/** @brief Half the side of the square that holds all of @p style, in px. */
auto HalfExtent(const CrosshairStyle& style) -> float {
  const float arms = std::max(style.gap + style.length, style.thickness * 0.5F);
  return std::max(arms, style.dot_radius) + style.outline + kAaMargin;
}

}  // namespace

HudSystem::HudSystem()
    : program_(loadProgram("vs_hud.sc", "fs_hud.sc")),
      quad_(CreateUnitQuad()),
      u_shape_(bgfx::createUniform("u_crossShape", bgfx::UniformType::Vec4)),
      u_extra_(bgfx::createUniform("u_crossExtra", bgfx::UniformType::Vec4)),
      u_color_(bgfx::createUniform("u_crossColor", bgfx::UniformType::Vec4)),
      u_outline_(
          bgfx::createUniform("u_crossOutline", bgfx::UniformType::Vec4)) {
  // Members are built, so throwing here still releases them
  if (!program_) {
    throw std::runtime_error("failed to link vs_hud/fs_hud program");
  }
}

void HudSystem::DrawCrosshair(const CrosshairStyle& style) {
  crosshair_ = style;
}

void HudSystem::Update(const FrameContext& ctx, bool visible) {
  bgfx::setViewRect(kHudView, 0, 0, static_cast<std::uint16_t>(ctx.width),
                    static_cast<std::uint16_t>(ctx.height));

  if (visible && crosshair_) {
    const CrosshairStyle& style = *crosshair_;
    const std::array<float, 4> shape{style.gap, style.length, style.thickness,
                                     style.outline};
    const std::array<float, 4> extra{style.dot_radius, HalfExtent(style), 0.0F,
                                     0.0F};
    bgfx::setUniform(u_shape_.Get(), shape.data());
    bgfx::setUniform(u_extra_.Get(), extra.data());
    bgfx::setUniform(u_color_.Get(), style.color.data());
    bgfx::setUniform(u_outline_.Get(), style.outline_color.data());

    bgfx::setVertexBuffer(0, quad_.Get());
    bgfx::setState(kHudState);
    bgfx::submit(kHudView, program_.Get());
  }
  crosshair_.reset();
}

}  // namespace engine

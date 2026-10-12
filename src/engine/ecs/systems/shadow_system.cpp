#include "engine/ecs/systems/shadow_system.h"

#include <bgfx/bgfx.h>
#include <bgfx/defines.h>
#include <bgfx_utils.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>

#include "engine/ecs/components/camera.h"
#include "engine/ecs/components/directional_light.h"
#include "engine/ecs/components/renderable.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/math/shadow_fit.h"
#include "engine/platform/frame_context.h"
#include "engine/render_views.h"
#include "engine/resource/asset_registry.h"
#include "engine/resource/gpu_mesh.h"
#include "engine/resource/unique_handle.h"

namespace engine {

namespace {

/// Shadow map texels per side.
constexpr std::uint16_t kResolution = 2048;
/// Metres from the camera that receive shadows.
constexpr float kShadowDistance = 40.0F;
/// Metres past the view toward the sun that still cast into it.
constexpr float kCasterMargin = 50.0F;
/// Receivers sample this many texels out along their normal, against acne.
constexpr float kNormalOffsetTexels = 1.5F;
/// Subtracted from the receiver's depth before comparing, 0..1.
constexpr float kDepthBias = 0.0002F;

constexpr std::uint64_t kMapFlags = BGFX_TEXTURE_RT |
                                    BGFX_SAMPLER_COMPARE_LEQUAL |
                                    BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;

// No culling: one-sided level meshes must cast from either side
constexpr std::uint64_t kCasterState =
    BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS;

/** @brief Creates a framebuffer over a new depth texture it owns. */
auto CreateShadowMap() -> UniqueHandle<bgfx::FrameBufferHandle> {
  const bgfx::TextureHandle depth = bgfx::createTexture2D(
      kResolution, kResolution, false, 1, bgfx::TextureFormat::D16, kMapFlags);
  return UniqueHandle(bgfx::createFrameBuffer(1, &depth, true));
}

}  // namespace

ShadowSystem::ShadowSystem()
    : program_(loadProgram("vs_shadow.sc", "fs_shadow.sc")),
      framebuffer_(CreateShadowMap()),
      u_shadow_mtx_(bgfx::createUniform("u_shadowMtx", bgfx::UniformFreq::Frame,
                                        bgfx::UniformType::Mat4)),
      u_shadow_params_(bgfx::createUniform("u_shadowParams",
                                           bgfx::UniformFreq::Frame,
                                           bgfx::UniformType::Vec4)) {
  // Members are built, so throwing here still releases them
  if (!program_) {
    throw std::runtime_error("failed to link vs_shadow/fs_shadow program");
  }
  if ((bgfx::getCaps()->supported & BGFX_CAPS_TEXTURE_COMPARE_LEQUAL) == 0) {
    throw std::runtime_error("renderer cannot sample shadow maps (no compare)");
  }

  bgfx::setViewRect(kShadowView, 0, 0, kResolution, kResolution);
  bgfx::setViewClear(kShadowView, BGFX_CLEAR_DEPTH, 0, 1.0F, 0);
}

void ShadowSystem::Update(Ecs& ecs, const AssetRegistry& assets,
                          const FrameContext& ctx) {
  // LightingSystem asserts there is at most one
  DirectionalLight light{};
  bool found_light = false;
  ecs.View<DirectionalLight>().ForEach(
      [&light, &found_light](Entity, const DirectionalLight& found) -> void {
        if (!found_light) {
          light = found;
          found_light = true;
        }
      });

  // No camera set: a default lens at the world origin, as RenderSystem draws
  const Transform pose = active_camera_
                             ? ecs.GetComponent<Transform>(*active_camera_)
                             : Transform{};
  const Camera lens =
      active_camera_ ? ecs.GetComponent<Camera>(*active_camera_) : Camera{};

  const bgfx::Caps* caps = bgfx::getCaps();
  const ShadowFitInput input{
      .eye = pose.position,
      .rotation = pose.rotation,
      .fov_degrees = lens.fov_degrees,
      .aspect = static_cast<float>(ctx.width) / static_cast<float>(ctx.height),
      .slice_near = lens.near_plane,
      .slice_far = std::min(kShadowDistance, lens.far_plane),
      .light_dir = light.direction,
      .resolution = kResolution,
      .caster_margin = kCasterMargin,
  };
  const LightMatrices fit =
      FitDirectionalShadow(input, caps->homogeneousDepth, kHandedness);

  const auto shadow_mtx =
      ShadowTextureMatrix(fit, caps->homogeneousDepth, caps->originBottomLeft);
  const std::array<float, 4> params{fit.texel_size * kNormalOffsetTexels,
                                    kDepthBias, 1.0F / kResolution, 0.0F};
  bgfx::setFrameUniform(u_shadow_mtx_.Get(), shadow_mtx.data());
  bgfx::setFrameUniform(u_shadow_params_.Get(), params.data());

  // bgfx::reset unbinds every view's framebuffer
  bgfx::setViewFrameBuffer(kShadowView, framebuffer_.Get());
  bgfx::setViewTransform(kShadowView, fit.view.data(), fit.proj.data());
  // Clears the map even when nothing casts
  bgfx::touch(kShadowView);

  ecs.View<Transform, Renderable>().ForEach(
      [&assets, this](Entity, const Transform& transform,
                      const Renderable& renderable) -> void {
        const GpuMesh& mesh = assets.GetMesh(renderable.mesh_handle);
        const auto model = ModelMatrix(transform);
        bgfx::setTransform(model.data());
        bgfx::setVertexBuffer(0, mesh.positions.Get());
        // Every submesh in one draw: depth ignores materials
        bgfx::setIndexBuffer(mesh.indices.Get());
        bgfx::setState(kCasterState);
        bgfx::submit(kShadowView, program_.Get());
      });
}

void ShadowSystem::SetActiveCamera(Entity camera) { active_camera_ = camera; }

auto ShadowSystem::Map() const -> bgfx::TextureHandle {
  return bgfx::getTexture(framebuffer_.Get());
}

}  // namespace engine

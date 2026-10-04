#include "ecs/systems/render_system.h"

#include <bgfx/bgfx.h>
#include <bgfx/defines.h>
#include <bgfx_utils.h>
#include <bx/math.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>

#include "ecs/components/camera.h"
#include "ecs/components/renderable.h"
#include "ecs/components/selected.h"
#include "ecs/components/transform.h"
#include "ecs/core/ecs.h"
#include "ecs/core/frame_context.h"
#include "ecs/core/types.h"
#include "resource/asset_registry.h"
#include "resource/texture_handle.h"

namespace {

// Matrix element count for bx's 4x4 routines.
constexpr std::size_t kMtxSize = 16;

constexpr uint32_t kClearColor = 0x303030ff;  // RGBA
constexpr std::array<float, 4> kHighlightColor{0.0F, 0.0F, 1.0F, 0.5F};
constexpr std::array<float, 4> kNoHighlight{0.0F, 0.0F, 0.0F, 0.0F};
// Only used when no camera entity has been nominated.
constexpr float kFallbackFovDegrees = 60.0F;
constexpr float kFallbackNearPlane = 0.1F;
constexpr float kFallbackFarPlane = 100.0F;

/** @brief Prints @p text to bgfx's debug overlay at a character cell. */
void DebugText(std::uint16_t x, std::uint16_t y, std::uint8_t attr,
               const std::string& text) {
  // Only vararg call; "%s" keeps the format fixed.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  bgfx::dbgTextPrintf(x, y, attr, "%s", text.c_str());
}

/** @brief Draws generic debug info to the window. */
auto DrawDebugOverlay(const FrameContext& ctx) -> void {
  const bgfx::Stats* stats = bgfx::getStats();
  bgfx::dbgTextClear();
  DebugText(0, 0, 0x0f,
            std::format("Backbuffer {}W x {}H", stats->width, stats->height));
  // First frame has dt == 0.
  const float fps = ctx.dt > 0.0F ? 1.0F / ctx.dt : 0.0F;
  DebugText(0, 1, 0x0f,
            std::format("Frame {:.2f} ms ({:.0f} fps)", ctx.dt * 1000.0F, fps));
}

/** @brief Creates the 1x1 opaque white texture bound for untextured draws. */
auto CreateWhiteTexture() -> bgfx::TextureHandle {
  constexpr uint32_t kWhite = 0xffffffff;
  return bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8,
                               BGFX_TEXTURE_NONE,
                               bgfx::copy(&kWhite, sizeof(kWhite)));
}
}  // namespace

RenderSystem::RenderSystem()
    : default_program_(loadProgram("vs_mesh.sc", "fs_mesh.sc")),
      u_time_(bgfx::createUniform("u_time", bgfx::UniformFreq::Frame,
                                  bgfx::UniformType::Vec4)),
      u_color_(bgfx::createUniform("u_color", bgfx::UniformType::Vec4)),
      u_highlight_(bgfx::createUniform("u_highlight", bgfx::UniformType::Vec4)),
      u_eye_pos_(bgfx::createUniform("u_eyePos", bgfx::UniformFreq::Frame,
                                     bgfx::UniformType::Vec4)),
      s_albedo_(bgfx::createUniform("s_albedo", bgfx::UniformType::Sampler)),
      u_tex_params_(
          bgfx::createUniform("u_texParams", bgfx::UniformType::Vec4)),
      default_texture_(CreateWhiteTexture()) {
  // Members are built, so throwing here still releases them
  if (!default_program_) {
    throw std::runtime_error("failed to link vs_mesh/fs_mesh program");
  }

  // View 0 clears the backbuffer each frame.
  bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, kClearColor, 1.0F,
                     0);
}

void RenderSystem::Update(Ecs& ecs, const AssetRegistry& assets,
                          const FrameContext& ctx) {
  DrawDebugOverlay(ctx);
  bgfx::setFrameUniform(u_time_.Get(), &ctx.time);

  // View and projection for view 0, written by CameraControl this frame
  {
    std::array<float, kMtxSize> view{};
    std::array<float, kMtxSize> proj{};
    std::array<float, 4> eye_pos{0.0F, 0.0F, 0.0F, 0.0F};

    if (camera_) {
      const auto& transform = ecs.GetComponent<Transform>(*camera_);
      const auto& camera = ecs.GetComponent<Camera>(*camera_);
      eye_pos = {transform.position.x, transform.position.y,
                 transform.position.z, 0.0F};
      view = camera.view;
      proj = camera.proj;
    } else {
      // No camera set: view from the world origin with the stock projection.
      const auto aspect =
          static_cast<float>(ctx.width) / static_cast<float>(ctx.height);
      bx::mtxIdentity(view.data());
      bx::mtxProj(proj.data(), kFallbackFovDegrees, aspect, kFallbackNearPlane,
                  kFallbackFarPlane, bgfx::getCaps()->homogeneousDepth);
    }
    bgfx::setFrameUniform(u_eye_pos_.Get(), eye_pos.data());

    bgfx::setViewTransform(0, view.data(), proj.data());
    bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(ctx.width),
                      static_cast<uint16_t>(ctx.height));
  }

  ecs.View<Transform, Renderable>().ForEach(
      [this, &ecs, &assets](Entity entity, const Transform& transform,
                            const Renderable& renderable) -> void {
        const auto mtx = ModelMatrix(transform);

        const auto program = bgfx::isValid(renderable.program)
                                 ? renderable.program
                                 : default_program_.Get();
        const auto* mesh = assets.GetMesh(renderable.mesh_handle);

        const auto texture = IsValid(renderable.texture)
                                 ? assets.GetTexture(renderable.texture)
                                 : default_texture_.Get();
        const std::array<float, 4> tex_params{1.0F / renderable.texture_scale,
                                              0.0F, 0.0F, 0.0F};

        // meshSubmit only discards state after its last group, so the color and
        // texture hold for every group of the mesh.
        bgfx::setUniform(u_color_.Get(), renderable.color.data());
        // TODO: How to avoid getting selected entity's Component?
        const auto highlight =
            ecs.HasComponent<Selected>(entity) ? kHighlightColor : kNoHighlight;
        bgfx::setUniform(u_highlight_.Get(), highlight.data());
        bgfx::setUniform(u_tex_params_.Get(), tex_params.data());
        bgfx::setTexture(0, s_albedo_.Get(), texture);
        meshSubmit(mesh, renderable.view, program, mtx.data(),
                   renderable.state);
      });

  bgfx::frame();
}

void RenderSystem::SetCamera(Entity camera) { camera_ = camera; }

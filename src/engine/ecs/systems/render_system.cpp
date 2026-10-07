#include "engine/ecs/systems/render_system.h"

#include <bgfx/bgfx.h>
#include <bgfx/defines.h>
#include <bgfx_utils.h>
#include <bx/math.h>

#include <array>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>

#include "engine/ecs/components/camera.h"
#include "engine/ecs/components/renderable.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/platform/frame_context.h"
#include "engine/resource/asset_registry.h"
#include "engine/resource/texture_handle.h"

namespace {

constexpr uint32_t kClearColor = 0x303030ff;  // RGBA

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

  // View and projection for view 0, from the camera's pose and lens
  {
    // No camera set: a default lens at the world origin
    const Transform pose =
        camera_ ? ecs.GetComponent<Transform>(*camera_) : Transform{};
    const Camera lens = camera_ ? ecs.GetComponent<Camera>(*camera_) : Camera{};

    const auto view = ViewMatrix(pose);
    const auto proj = ProjectionMatrix(
        lens, static_cast<float>(ctx.width) / static_cast<float>(ctx.height),
        bgfx::getCaps()->homogeneousDepth);
    const std::array<float, 4> eye_pos{pose.position.x, pose.position.y,
                                       pose.position.z, 0.0F};
    bgfx::setFrameUniform(u_eye_pos_.Get(), eye_pos.data());

    bgfx::setViewTransform(0, view.data(), proj.data());
    bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(ctx.width),
                      static_cast<uint16_t>(ctx.height));
  }

  ecs.View<Transform, Renderable>().ForEach(
      [this, &assets](Entity /*entity*/, const Transform& transform,
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
        bgfx::setUniform(u_tex_params_.Get(), tex_params.data());
        bgfx::setTexture(0, s_albedo_.Get(), texture);
        meshSubmit(mesh, renderable.view, program, mtx.data(),
                   renderable.state);
      });

  bgfx::frame();
}

void RenderSystem::SetCamera(Entity camera) { camera_ = camera; }

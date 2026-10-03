#include "ecs/systems/render_system.h"

#include <bgfx/bgfx.h>
#include <bgfx/defines.h>
#include <bgfx_utils.h>
#include <bx/math.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>

#include "ecs/components/camera.h"
#include "ecs/components/renderable.h"
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

}  // namespace

void RenderSystem::Init(const AssetRegistry& assets) {
  assets_ = &assets;

  // View 0 clears the backbuffer each frame.
  bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, kClearColor, 1.0F,
                     0);

  u_time_ = bgfx::createUniform("u_time", bgfx::UniformFreq::Frame,
                                bgfx::UniformType::Vec4);
  u_color_ = bgfx::createUniform("u_color", bgfx::UniformType::Vec4);
  u_eye_pos_ = bgfx::createUniform("u_eyePos", bgfx::UniformFreq::Frame,
                                   bgfx::UniformType::Vec4);
  s_albedo_ = bgfx::createUniform("s_albedo", bgfx::UniformType::Sampler);
  u_tex_params_ = bgfx::createUniform("u_texParams", bgfx::UniformType::Vec4);
  default_program_ = loadProgram("vs_mesh.sc", "fs_mesh.sc");

  constexpr uint32_t kWhite = 0xffffffff;
  default_texture_ = bgfx::createTexture2D(
      1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE,
      bgfx::copy(&kWhite, sizeof(kWhite)));
}

void RenderSystem::Update(Ecs& ecs, const FrameContext& ctx) {
  // Debug overlay.
  const bgfx::Stats* stats = bgfx::getStats();
  bgfx::dbgTextClear();
  DebugText(0, 3, 0x0f,
            std::format("Backbuffer {}W x {}H", stats->width, stats->height));
  // First frame has dt == 0.
  const float fps = ctx.dt > 0.0F ? 1.0F / ctx.dt : 0.0F;
  DebugText(0, 4, 0x0f,
            std::format("Frame {:.2f} ms ({:.0f} fps)", ctx.dt * 1000.0F, fps));

  bgfx::setFrameUniform(u_time_, &ctx.time);

  // View and projection for view 0 taken from the camera entity's components.
  // The CameraControl system writes those each frame.
  {
    const auto aspect =
        static_cast<float>(ctx.width) / static_cast<float>(ctx.height);

    std::array<float, kMtxSize> view{};
    std::array<float, kMtxSize> proj{};
    std::array<float, 4> eye_pos{0.0F, 0.0F, 0.0F, 0.0F};

    if (has_camera_) {
      const auto& transform = ecs.GetComponent<Transform>(camera_);
      const auto& camera = ecs.GetComponent<Camera>(camera_);
      eye_pos = {transform.position.x, transform.position.y,
                 transform.position.z, 0.0F};

      bx::mtxLookAt(view.data(), transform.position, camera.target, camera.up);
      bx::mtxProj(proj.data(), camera.fov_degrees, aspect, camera.near_plane,
                  camera.far_plane, bgfx::getCaps()->homogeneousDepth);
    } else {
      // No camera set: view from the world origin with the stock projection.
      bx::mtxIdentity(view.data());
      bx::mtxProj(proj.data(), kFallbackFovDegrees, aspect, kFallbackNearPlane,
                  kFallbackFarPlane, bgfx::getCaps()->homogeneousDepth);
    }

    bgfx::setViewTransform(0, view.data(), proj.data());
    bgfx::setFrameUniform(u_eye_pos_, eye_pos.data());
    bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(ctx.width),
                      static_cast<uint16_t>(ctx.height));
  }

  ecs.View<Transform, Renderable>().ForEach(
      [this](Entity, const Transform& transform,
             const Renderable& renderable) -> void {
        std::array<float, kMtxSize> mtx{};
        bx::mtxSRT(
            mtx.data(), transform.scale.x, transform.scale.y, transform.scale.z,
            transform.rotation.x, transform.rotation.y, transform.rotation.z,
            transform.position.x, transform.position.y, transform.position.z);

        const auto program = bgfx::isValid(renderable.program)
                                 ? renderable.program
                                 : default_program_;
        const auto* mesh = assets_->GetMesh(renderable.mesh_handle);

        const auto texture = IsValid(renderable.texture)
                                 ? assets_->GetTexture(renderable.texture)
                                 : default_texture_;
        const std::array<float, 4> tex_params{1.0F / renderable.texture_scale,
                                              0.0F, 0.0F, 0.0F};

        // meshSubmit only discards state after its last group, so the color and
        // texture hold for every group of the mesh.
        bgfx::setUniform(u_color_, renderable.color.data());
        bgfx::setUniform(u_tex_params_, tex_params.data());
        bgfx::setTexture(0, s_albedo_, texture);
        meshSubmit(mesh, renderable.view, program, mtx.data(),
                   renderable.state);
      });

  bgfx::frame();
}

void RenderSystem::Shutdown() {
  bgfx::destroy(u_time_);
  bgfx::destroy(u_color_);
  bgfx::destroy(u_eye_pos_);
  bgfx::destroy(s_albedo_);
  bgfx::destroy(u_tex_params_);
  bgfx::destroy(default_texture_);
  bgfx::destroy(default_program_);
}

void RenderSystem::SetCamera(Entity camera) {
  camera_ = camera;
  has_camera_ = true;
}

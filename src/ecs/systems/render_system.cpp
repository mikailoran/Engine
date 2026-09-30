#include "render_system.h"

#include "../../resource/asset_registry.h"
#include "../components/camera.h"
#include "../components/renderable.h"
#include "../components/transform.h"
#include "../core/ecs.h"
#include "../core/frame_context.h"

#include <array>
#include <bgfx_utils.h>
#include <bx/math.h>

namespace {

// Matrix element count for bx's 4x4 routines.
constexpr std::size_t kMtxSize = 16;

constexpr uint32_t kClearColor = 0x303030ff; // RGBA
// Only used when no camera entity has been nominated.
constexpr float kFallbackFovDegrees = 60.0F;
constexpr float kFallbackNearPlane = 0.1F;
constexpr float kFallbackFarPlane = 100.0F;

// Fixed sun and hemisphere ambient, linear RGB; w unused.
constexpr bx::Vec3 kSunDirection{-0.4F, 1.0F, -0.3F}; // toward the sun
constexpr std::array<float, 4> kSunColor{1.0F, 0.95F, 0.85F, 0.0F};
constexpr std::array<float, 4> kSkyColor{0.22F, 0.25F, 0.3F, 0.0F};
constexpr std::array<float, 4> kGroundColor{0.08F, 0.07F, 0.06F, 0.0F};

} // namespace

void RenderSystem::Init(const AssetRegistry &assets) {
  assets_ = &assets;

  // View 0 clears the backbuffer each frame.
  bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, kClearColor, 1.0F,
                     0);

  u_time_ = bgfx::createUniform("u_time", bgfx::UniformFreq::Frame,
                                bgfx::UniformType::Vec4);
  u_color_ = bgfx::createUniform("u_color", bgfx::UniformType::Vec4);
  u_eye_pos_ = bgfx::createUniform("u_eyePos", bgfx::UniformFreq::Frame,
                                   bgfx::UniformType::Vec4);
  u_light_dir_ = bgfx::createUniform("u_lightDir", bgfx::UniformFreq::Frame,
                                     bgfx::UniformType::Vec4);
  u_light_color_ = bgfx::createUniform(
      "u_lightColor", bgfx::UniformFreq::Frame, bgfx::UniformType::Vec4);
  u_sky_color_ = bgfx::createUniform("u_skyColor", bgfx::UniformFreq::Frame,
                                     bgfx::UniformType::Vec4);
  u_ground_color_ = bgfx::createUniform(
      "u_groundColor", bgfx::UniformFreq::Frame, bgfx::UniformType::Vec4);
  default_program_ = loadProgram("vs_mesh.sc", "fs_mesh.sc");

}

void RenderSystem::Update(Ecs &ecs, const FrameContext &ctx) {
  // Debug overlay.
  const bgfx::Stats *stats = bgfx::getStats();
  bgfx::dbgTextClear();
  bgfx::dbgTextPrintf(0, 3, 0x0f, "Backbuffer %dW x %dH", stats->width,
                      stats->height);

  bgfx::setFrameUniform(u_time_, &ctx.time);

  // Sun and ambient are constant until lights become components.
  {
    const bx::Vec3 dir = bx::normalize(kSunDirection);
    const std::array<float, 4> light_dir{dir.x, dir.y, dir.z, 0.0F};
    bgfx::setFrameUniform(u_light_dir_, light_dir.data());
    bgfx::setFrameUniform(u_light_color_, kSunColor.data());
    bgfx::setFrameUniform(u_sky_color_, kSkyColor.data());
    bgfx::setFrameUniform(u_ground_color_, kGroundColor.data());
  }

  // View and projection for view 0 taken from the camera entity's components.
  // The CameraControl system writes those each frame.
  {
    const auto aspect =
        static_cast<float>(ctx.width) / static_cast<float>(ctx.height);

    std::array<float, kMtxSize> view{};
    std::array<float, kMtxSize> proj{};
    std::array<float, 4> eye_pos{0.0F, 0.0F, 0.0F, 0.0F};

    if (has_camera_) {
      const auto &transform = ecs.GetComponent<Transform>(camera_);
      const auto &camera = ecs.GetComponent<Camera>(camera_);
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

  for (const auto &entity : entities) {
    const auto &transform = ecs.GetComponent<Transform>(entity);
    const auto &renderable = ecs.GetComponent<Renderable>(entity);

    std::array<float, kMtxSize> mtx{};
    bx::mtxSRT(mtx.data(), transform.scale.x, transform.scale.y,
               transform.scale.z, transform.rotation.x, transform.rotation.y,
               transform.rotation.z, transform.position.x, transform.position.y,
               transform.position.z);

    const auto program = bgfx::isValid(renderable.program) ? renderable.program
                                                           : default_program_;
    const auto *mesh = assets_->Get(renderable.mesh_handle);

    // meshSubmit only discards state after its last group, so the color holds
    // for every group of the mesh.
    bgfx::setUniform(u_color_, renderable.color.data());
    meshSubmit(mesh, renderable.view, program, mtx.data(), renderable.state);
  }

  bgfx::frame();
}

void RenderSystem::Shutdown() {
  bgfx::destroy(u_time_);
  bgfx::destroy(u_color_);
  bgfx::destroy(u_eye_pos_);
  bgfx::destroy(u_light_dir_);
  bgfx::destroy(u_light_color_);
  bgfx::destroy(u_sky_color_);
  bgfx::destroy(u_ground_color_);
  bgfx::destroy(default_program_);
}

void RenderSystem::SetCamera(Entity camera) {
  camera_ = camera;
  has_camera_ = true;
}

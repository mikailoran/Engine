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

/**
 * @brief Position + normal vertex, matching what the vertex shader expects.
 *
 */
struct FloorVertex {
  float m_x;
  float m_y;
  float m_z;
  float m_nx;
  float m_ny;
  float m_nz;

  static void init() {
    ms_layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .end();
  }

  static bgfx::VertexLayout ms_layout;
};
bgfx::VertexLayout FloorVertex::ms_layout;

// A flat quad in the XZ plane representing the floor.
constexpr std::array<FloorVertex, 4> kFloorVertices{{
    {-10.0F, 0.0F, -10.0F, 0.5F, 1.0F, 0.5F},
    {10.0F, 0.0F, -10.0F, 0.5F, 1.0F, 0.5F},
    {10.0F, 0.0F, 10.0F, 0.5F, 1.0F, 0.5F},
    {-10.0F, 0.0F, 10.0F, 0.5F, 1.0F, 0.5F},
}};
constexpr std::array<uint16_t, 6> kFloorIndices{0, 1, 2, 0, 2, 3};

} // namespace

void RenderSystem::Init(const AssetRegistry &assets) {
  assets_ = &assets;

  // View 0 clears the backbuffer each frame.
  bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, kClearColor, 1.0F,
                     0);

  u_time_ = bgfx::createUniform("u_time", bgfx::UniformFreq::Frame,
                                bgfx::UniformType::Vec4);
  default_program_ = loadProgram("vs.sc", "fs.sc");

  SetupFloor();
}

void RenderSystem::Update(Ecs &ecs, const FrameContext &ctx) {
  // Debug overlay.
  const bgfx::Stats *stats = bgfx::getStats();
  bgfx::dbgTextClear();
  bgfx::dbgTextPrintf(0, 3, 0x0f, "Backbuffer %dW x %dH", stats->width,
                      stats->height);

  bgfx::setFrameUniform(u_time_, &ctx.time);

  // View and projection for view 0 taken from the camera entity's components.
  // The CameraControl system writes those each frame.
  {
    const auto aspect =
        static_cast<float>(ctx.width) / static_cast<float>(ctx.height);

    std::array<float, kMtxSize> view{};
    std::array<float, kMtxSize> proj{};

    if (has_camera_) {
      const auto &transform = ecs.GetComponent<Transform>(camera_);
      const auto &camera = ecs.GetComponent<Camera>(camera_);

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

    meshSubmit(mesh, renderable.view, program, mtx.data(), renderable.state);
  }

  SubmitFloor();

  bgfx::frame();
}

void RenderSystem::Shutdown() {
  bgfx::destroy(floor_vbh_);
  bgfx::destroy(floor_ibh_);
  bgfx::destroy(u_time_);
  bgfx::destroy(default_program_);
}

void RenderSystem::SetCamera(Entity camera) {
  camera_ = camera;
  has_camera_ = true;
}

void RenderSystem::SetupFloor() {
  // bgfx::copy rather than makeRef: makeRef would require the source arrays to
  // outlive the buffers, which is fragile now that they are function-local to
  // this translation unit.
  FloorVertex::init();
  floor_vbh_ = bgfx::createVertexBuffer(
      bgfx::copy(kFloorVertices.data(),
                 kFloorVertices.size() * sizeof(FloorVertex)),
      FloorVertex::ms_layout);
  floor_ibh_ = bgfx::createIndexBuffer(bgfx::copy(
      kFloorIndices.data(), kFloorIndices.size() * sizeof(uint16_t)));
}

void RenderSystem::SubmitFloor() {
  // Culling is disabled so winding order cannot hide it.
  // Using identity matrix explicitly.
  std::array<float, kMtxSize> floor_mtx{};
  bx::mtxIdentity(floor_mtx.data());
  bgfx::setTransform(floor_mtx.data());
  bgfx::setVertexBuffer(0, floor_vbh_);
  bgfx::setIndexBuffer(floor_ibh_);
  bgfx::setState(BGFX_STATE_DEFAULT & ~BGFX_STATE_CULL_MASK);
  bgfx::submit(0, default_program_);
}
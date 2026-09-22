#include "render_system.h"

#include "../components/renderable.h"
#include "../components/transform.h"
#include "../core/ecs.h"
#include "../core/frame_context.h"

#include <array>
#include <bgfx_utils.h>
#include <bx/math.h>
#include <camera.h>

namespace {

/// Matrix element count for bx's 4x4 routines.
constexpr std::size_t kMtxSize = 16;

constexpr uint32_t kClearColor = 0x303030ff; // RGBA
constexpr float kFovDegrees = 60.0F;
constexpr float kNearPlane = 0.1F;
constexpr float kFarPlane = 100.0F;

/**
 * @brief Position + normal vertex, laid out to match what vs.sc expects.
 *
 * vs.sc decodes normals with `a_normal.xyz*2.0 - 1.0`, the standard unpack for
 * a normal stored in [0,1] (the convention used by packed-normal meshes like
 * the loaded bunny). Raw floats are used here rather than packed bytes, but
 * they still have to be pre-biased into [0,1] so that decode step recovers the
 * intended [-1,1] normal.
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

// A single flat quad in the XZ plane (Y up), normal pre-biased to (0.5, 1.0,
// 0.5) so vs.sc's decode yields a straight-up (0, 1, 0) normal.
constexpr std::array<FloorVertex, 4> kFloorVertices{{
    {-10.0F, 0.0F, -10.0F, 0.5F, 1.0F, 0.5F},
    {10.0F, 0.0F, -10.0F, 0.5F, 1.0F, 0.5F},
    {10.0F, 0.0F, 10.0F, 0.5F, 1.0F, 0.5F},
    {-10.0F, 0.0F, 10.0F, 0.5F, 1.0F, 0.5F},
}};
constexpr std::array<uint16_t, 6> kFloorIndices{0, 1, 2, 0, 2, 3};

} // namespace

void RenderSystem::Init() {
  // View 0 clears the backbuffer each frame.
  bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, kClearColor, 1.0F,
                     0);

  u_time_ = bgfx::createUniform("u_time", bgfx::UniformFreq::Frame,
                                bgfx::UniformType::Vec4);
  default_program_ = loadProgram("vs.sc", "fs.sc");

  // Static floor quad: layout only needs registering once before use.
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

void RenderSystem::Update(Ecs &ecs, const FrameContext &ctx) {
  // Debug overlay.
  const bgfx::Stats *stats = bgfx::getStats();
  bgfx::dbgTextClear();
  bgfx::dbgTextPrintf(0, 3, 0x0f, "Backbuffer %dW x %dH", stats->width,
                      stats->height);

  bgfx::setFrameUniform(u_time_, &ctx.time);

  // View and projection for view 0.
  {
    std::array<float, kMtxSize> view{};
    // TODO(Phase 4): source this from the camera entity's Transform + Camera.
    cameraGetViewMtx(view.data());

    const auto aspect =
        static_cast<float>(ctx.width) / static_cast<float>(ctx.height);

    std::array<float, kMtxSize> proj{};
    bx::mtxProj(proj.data(), kFovDegrees, aspect, kNearPlane, kFarPlane,
                bgfx::getCaps()->homogeneousDepth);

    bgfx::setViewTransform(0, view.data(), proj.data());
    bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(ctx.width),
                      static_cast<uint16_t>(ctx.height));
  }

  // Signature is {Transform, Renderable}, so both are guaranteed present.
  for (const auto &entity : entities) {
    const auto &transform = ecs.GetComponent<Transform>(entity);
    const auto &renderable = ecs.GetComponent<Renderable>(entity);

    // Full scale-rotate-translate, so position and scale both take effect.
    std::array<float, kMtxSize> mtx{};
    bx::mtxSRT(mtx.data(), transform.scale.x, transform.scale.y,
               transform.scale.z, transform.rotation.x, transform.rotation.y,
               transform.rotation.z, transform.position.x, transform.position.y,
               transform.position.z);

    const auto program = bgfx::isValid(renderable.program) ? renderable.program
                                                           : default_program_;

    meshSubmit(renderable.mesh, renderable.view, program, mtx.data(),
               renderable.state);
  }

  // Floor: static, at the origin, so an identity transform is enough. Culling
  // is disabled for this draw so winding order cannot hide it.
  std::array<float, kMtxSize> floor_mtx{};
  bx::mtxIdentity(floor_mtx.data());
  bgfx::setTransform(floor_mtx.data());
  bgfx::setVertexBuffer(0, floor_vbh_);
  bgfx::setIndexBuffer(floor_ibh_);
  bgfx::setState(BGFX_STATE_DEFAULT & ~BGFX_STATE_CULL_MASK);
  bgfx::submit(0, default_program_);

  // Advance to the next frame; kicks the render thread.
  bgfx::frame();
}

void RenderSystem::Shutdown(Ecs &ecs) {
  // Each Renderable owns its mesh.
  for (const auto &entity : entities) {
    auto &renderable = ecs.GetComponent<Renderable>(entity);
    if (renderable.mesh != nullptr) {
      meshUnload(renderable.mesh);
      renderable.mesh = nullptr;
    }
  }

  bgfx::destroy(floor_vbh_);
  bgfx::destroy(floor_ibh_);
  bgfx::destroy(u_time_);
  bgfx::destroy(default_program_);
}

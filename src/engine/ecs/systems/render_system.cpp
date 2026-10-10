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
#include <vector>

#include "engine/ecs/components/camera.h"
#include "engine/ecs/components/renderable.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/platform/frame_context.h"
#include "engine/resource/asset_registry.h"
#include "engine/resource/cpu_mesh.h"
#include "engine/resource/gpu_mesh.h"

namespace engine {

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

/** @brief One submesh to draw this frame, and what to draw it with. */
struct DrawItem {
  /// The entity's model matrix, from its Transform.
  std::array<float, 16> model{};
  /// A copy of the entity's Renderable.
  Renderable renderable;
  /// Index into the mesh's submeshes.
  std::uint32_t submesh{0};
};

/**
 * @brief One draw item per submesh of every entity with a Transform and a
 * Renderable, in view order.
 */
auto GatherDraws(Ecs& ecs, const AssetRegistry& assets)
    -> std::vector<DrawItem> {
  std::vector<DrawItem> draws;
  ecs.View<Transform, Renderable>().ForEach(
      [&draws, &assets](Entity /*entity*/, const Transform& transform,
                        const Renderable& renderable) -> void {
        const auto model = ModelMatrix(transform);
        const GpuMesh& mesh = assets.GetMesh(renderable.mesh_handle);
        for (std::uint32_t i = 0; i < mesh.submeshes.size(); ++i) {
          draws.push_back(
              {.model = model, .renderable = renderable, .submesh = i});
        }
      });
  return draws;
}

}  // namespace

RenderSystem::RenderSystem()
    : default_program_(loadProgram("vs_mesh.sc", "fs_mesh.sc")),
      u_time_(bgfx::createUniform("u_time", bgfx::UniformFreq::Frame,
                                  bgfx::UniformType::Vec4)),
      u_color_(bgfx::createUniform("u_color", bgfx::UniformType::Vec4)),
      u_eye_pos_(bgfx::createUniform("u_eyePos", bgfx::UniformFreq::Frame,
                                     bgfx::UniformType::Vec4)) {
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
    const Transform pose = active_camera_
                               ? ecs.GetComponent<Transform>(*active_camera_)
                               : Transform{};
    const Camera lens =
        active_camera_ ? ecs.GetComponent<Camera>(*active_camera_) : Camera{};

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

  // Gather first, then submit: instancing will batch between the two
  for (const DrawItem& draw : GatherDraws(ecs, assets)) {
    const Renderable& renderable = draw.renderable;
    const GpuMesh& mesh = assets.GetMesh(renderable.mesh_handle);
    const Submesh& submesh = mesh.submeshes.at(draw.submesh);

    const auto program = bgfx::isValid(renderable.program)
                             ? renderable.program
                             : default_program_.Get();

    bgfx::setTransform(draw.model.data());
    bgfx::setVertexBuffer(0, mesh.positions.Get());
    bgfx::setVertexBuffer(1, mesh.normals.Get());
    bgfx::setVertexBuffer(2, mesh.uvs.Get());
    bgfx::setIndexBuffer(mesh.indices.Get(), submesh.first_index,
                         submesh.index_count);
    bgfx::setUniform(u_color_.Get(), renderable.color.data());
    bgfx::setState(renderable.state);
    bgfx::submit(renderable.view, program);
  }

  bgfx::frame();
}

void RenderSystem::SetActiveCamera(Entity camera) { active_camera_ = camera; }

}  // namespace engine

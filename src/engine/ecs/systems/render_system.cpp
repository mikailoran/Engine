#include "engine/ecs/systems/render_system.h"

#include <bgfx/bgfx.h>
#include <bgfx/defines.h>
#include <bgfx_utils.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>

#include "engine/ecs/components/camera.h"
#include "engine/ecs/components/renderable.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/math/vector.h"
#include "engine/platform/frame_context.h"
#include "engine/render_views.h"
#include "engine/resource/asset_registry.h"
#include "engine/resource/cpu_mesh.h"
#include "engine/resource/gpu_mesh.h"

namespace engine {

namespace {

constexpr uint32_t kClearColor = 0x303030ff;  // RGBA

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

/** @brief Creates the 1x1 opaque white texture bound for untextured draws. */
auto CreateWhiteTexture() -> bgfx::TextureHandle {
  constexpr uint32_t kWhite = 0xffffffff;
  return bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8,
                               BGFX_TEXTURE_NONE,
                               bgfx::copy(&kWhite, sizeof(kWhite)));
}

/** @brief Multiplies two RGBA colors channel by channel. */
auto Modulate(const std::array<float, 4>& a, const std::array<float, 4>& b)
    -> std::array<float, 4> {
  std::array<float, 4> product{};
  std::ranges::transform(a, b, product.begin(), std::multiplies<>{});
  return product;
}

}  // namespace

RenderSystem::RenderSystem()
    : default_program_(loadProgram("vs_mesh.sc", "fs_mesh.sc")),
      u_time_(bgfx::createUniform("u_time", bgfx::UniformFreq::Frame,
                                  bgfx::UniformType::Vec4)),
      u_color_(bgfx::createUniform("u_color", bgfx::UniformType::Vec4)),
      u_material_(bgfx::createUniform("u_material", bgfx::UniformType::Vec4)),
      u_eye_pos_(bgfx::createUniform("u_eyePos", bgfx::UniformFreq::Frame,
                                     bgfx::UniformType::Vec4)),
      s_albedo_(bgfx::createUniform("s_albedo", bgfx::UniformType::Sampler)),
      s_shadow_map_(
          bgfx::createUniform("s_shadowMap", bgfx::UniformType::Sampler)),
      default_texture_(CreateWhiteTexture()) {
  // Members are built, so throwing here still releases them
  if (!default_program_) {
    throw std::runtime_error("failed to link vs_mesh/fs_mesh program");
  }

  // The scene view clears the backbuffer each frame
  bgfx::setViewClear(kSceneView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                     kClearColor, 1.0F, 0);
}

void RenderSystem::Update(Ecs& ecs, const AssetRegistry& assets,
                          const FrameContext& ctx,
                          bgfx::TextureHandle shadow_map) {
  bgfx::setFrameUniform(u_time_.Get(), &ctx.time);

  // View and projection for the scene view, from the camera's pose and lens
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
    const auto eye_pos = ToVec4(pose.position, 1.0F);
    bgfx::setFrameUniform(u_eye_pos_.Get(), eye_pos.data());

    bgfx::setViewTransform(kSceneView, view.data(), proj.data());
    bgfx::setViewRect(kSceneView, 0, 0, static_cast<uint16_t>(ctx.width),
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
    // Primitives without a glTF material draw the default (white, untextured)
    const MeshMaterial material = submesh.material
                                      ? mesh.materials.at(*submesh.material)
                                      : MeshMaterial{};
    const auto texture =
        material.base_color_image
            ? mesh.textures.at(*material.base_color_image).Get()
            : default_texture_.Get();
    const auto color = Modulate(material.base_color, renderable.color);
    const std::array<float, 4> factors{material.metallic, material.roughness,
                                       0.0F, 0.0F};

    bgfx::setUniform(u_color_.Get(), color.data());
    bgfx::setUniform(u_material_.Get(), factors.data());
    bgfx::setTexture(0, s_albedo_.Get(), texture);
    bgfx::setTexture(1, s_shadow_map_.Get(), shadow_map);
    bgfx::setState(renderable.state);
    bgfx::submit(renderable.view, program);
  }

  bgfx::frame();
}

void RenderSystem::SetActiveCamera(Entity camera) { active_camera_ = camera; }

}  // namespace engine

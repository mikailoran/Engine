#include "engine/ecs/systems/sky_system.h"

#include <bgfx/bgfx.h>
#include <bgfx/defines.h>
#include <bgfx_utils.h>
#include <bx/math.h>

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "engine/ecs/components/sky.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/math/vector.h"
#include "engine/render_views.h"
#include "engine/resource/unique_handle.h"

namespace engine {

namespace {

// No depth write: depth 1 passes only where the clear value remains
constexpr std::uint64_t kSkyState =
    BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_MSAA;

/** @brief Uploads a clip-space triangle that covers the whole viewport. */
auto CreateFullscreenTriangle() -> UniqueHandle<bgfx::VertexBufferHandle> {
  constexpr std::array<float, 9> kVertices{
      -1.0F, -1.0F, 0.0F,  //
      3.0F,  -1.0F, 0.0F,  //
      -1.0F, 3.0F,  0.0F,
  };
  bgfx::VertexLayout layout;
  layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
  return UniqueHandle(bgfx::createVertexBuffer(
      bgfx::copy(kVertices.data(), sizeof(kVertices)), layout));
}

}  // namespace

SkySystem::SkySystem()
    : program_(loadProgram("vs_sky.sc", "fs_sky.sc")),
      triangle_(CreateFullscreenTriangle()),
      u_zenith_(bgfx::createUniform("u_skyZenith", bgfx::UniformType::Vec4)),
      u_horizon_(bgfx::createUniform("u_skyHorizon", bgfx::UniformType::Vec4)),
      u_ground_(bgfx::createUniform("u_skyGround", bgfx::UniformType::Vec4)),
      u_params_(bgfx::createUniform("u_skyParams", bgfx::UniformType::Vec4)) {
  // Members are built, so throwing here still releases them
  if (!program_) {
    throw std::runtime_error("failed to link vs_sky/fs_sky program");
  }
}

void SkySystem::Update(Ecs& ecs) {
  // Release builds use the first sky in dense order if several exist
  Sky sky{};
  std::size_t sky_count = 0;
  ecs.View<Sky>().ForEach([&sky, &sky_count](Entity, const Sky& found) -> void {
    if (sky_count++ == 0) {
      sky = found;
    }
  });
  assert(sky_count <= 1 && "at most one Sky per scene");

  const auto zenith = ToVec4(sky.zenith);
  const auto horizon = ToVec4(sky.horizon);
  const auto ground = ToVec4(sky.ground);
  const std::array<float, 4> params{std::cos(bx::toRad(sky.sun_radius_deg)),
                                    sky.glow_sharpness, sky.gradient_falloff,
                                    sky.glow_strength};
  bgfx::setUniform(u_zenith_.Get(), zenith.data());
  bgfx::setUniform(u_horizon_.Get(), horizon.data());
  bgfx::setUniform(u_ground_.Get(), ground.data());
  bgfx::setUniform(u_params_.Get(), params.data());

  bgfx::setVertexBuffer(0, triangle_.Get());
  bgfx::setState(kSkyState);
  // Shares the scene's view, behind it by depth
  bgfx::submit(kSceneView, program_.Get());
}

}  // namespace engine

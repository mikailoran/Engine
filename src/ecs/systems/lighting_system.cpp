#include "ecs/systems/lighting_system.h"

#include <bgfx/bgfx.h>
#include <bx/math.h>

#include <array>
#include <cassert>
#include <cstddef>

#include "ecs/components/directional_light.h"
#include "ecs/core/ecs.h"
#include "ecs/core/frame_context.h"
#include "ecs/core/types.h"

namespace {

/** @brief Packs a Vec3 into a vec4 uniform value with w = 0. */
auto ToVec4(const bx::Vec3& v) -> std::array<float, 4> {
  return {v.x, v.y, v.z, 0.0F};
}

}  // namespace

LightingSystem::LightingSystem()
    : u_light_dir_(bgfx::createUniform("u_lightDir", bgfx::UniformFreq::Frame,
                                       bgfx::UniformType::Vec4)),
      u_light_color_(bgfx::createUniform(
          "u_lightColor", bgfx::UniformFreq::Frame, bgfx::UniformType::Vec4)),
      u_sky_color_(bgfx::createUniform("u_skyColor", bgfx::UniformFreq::Frame,
                                       bgfx::UniformType::Vec4)),
      u_ground_color_(bgfx::createUniform(
          "u_groundColor", bgfx::UniformFreq::Frame, bgfx::UniformType::Vec4)) {
}

void LightingSystem::Update(Ecs& ecs, const FrameContext& /*ctx*/) {
  // Release builds use the first light in dense order if several exist
  DirectionalLight light{};
  std::size_t light_count = 0;
  ecs.View<DirectionalLight>().ForEach(
      [&light, &light_count](Entity, const DirectionalLight& found) -> void {
        if (light_count++ == 0) {
          light = found;
        }
      });
  assert(light_count <= 1 && "at most one DirectionalLight per scene");

  const auto light_dir = ToVec4(bx::normalize(light.direction));
  const auto light_color = ToVec4(bx::mul(light.color, light.intensity));
  const auto sky_color = ToVec4(light.sky_color);
  const auto ground_color = ToVec4(light.ground_color);
  bgfx::setFrameUniform(u_light_dir_.Get(), light_dir.data());
  bgfx::setFrameUniform(u_light_color_.Get(), light_color.data());
  bgfx::setFrameUniform(u_sky_color_.Get(), sky_color.data());
  bgfx::setFrameUniform(u_ground_color_.Get(), ground_color.data());
}

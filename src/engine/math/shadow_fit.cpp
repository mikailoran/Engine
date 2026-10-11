#include "engine/math/shadow_fit.h"

#include <bx/math.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

namespace engine {

namespace {

/// Above this |y| of the light direction, +Y is too close to serve as up.
constexpr float kVerticalLight = 0.99F;

/** @brief A sphere around the slice, centred on the camera's view axis. */
struct SliceSphere {
  /// Distance from the eye to the centre along the view axis, in metres.
  float center{0.0F};
  float radius{0.0F};
};

/** @brief The smallest sphere around the slice whose centre is on its axis. */
auto BoundSlice(const ShadowFitInput& input) -> SliceSphere {
  const float n = input.slice_near;
  const float f = input.slice_far;
  // Squared distance of a corner from the axis, per metre along it
  const float tan_half = std::tan(bx::toRad(input.fov_degrees) * 0.5F);
  const float k2 = tan_half * tan_half * (1.0F + (input.aspect * input.aspect));

  // Equidistant from near and far corners, unless that is past the far plane
  const float center = std::min((1.0F + k2) * (n + f) * 0.5F, f);
  const float radius = std::sqrt(((f - center) * (f - center)) + (f * f * k2));
  return {.center = center, .radius = radius};
}

}  // namespace

auto FitDirectionalShadow(const ShadowFitInput& input, bool homogeneous_depth,
                          bx::Handedness::Enum handedness) -> LightMatrices {
  assert(input.slice_near >= 0.0F && input.slice_far > input.slice_near);
  assert(input.fov_degrees > 0.0F && input.fov_degrees < 180.0F);
  assert(input.aspect > 0.0F && input.resolution > 0);
  assert(bx::length(input.light_dir) > 0.0F && "light has no direction");

  const bx::Vec3 to_light = bx::normalize(input.light_dir);
  const bx::Vec3 up = std::abs(to_light.y) > kVerticalLight
                          ? bx::Vec3{0.0F, 0.0F, 1.0F}
                          : bx::Vec3{0.0F, 1.0F, 0.0F};

  // Rotation only, so a texel grid snapped in this frame is fixed in the world
  LightMatrices light;
  bx::mtxLookAt(light.view.data(), {0.0F, 0.0F, 0.0F}, bx::neg(to_light), up,
                handedness);

  const SliceSphere sphere = BoundSlice(input);
  const bx::Vec3 forward = bx::mul(bx::Vec3{0.0F, 0.0F, -1.0F}, input.rotation);
  const bx::Vec3 center = bx::mul(
      bx::add(input.eye, bx::mul(forward, sphere.center)), light.view.data());

  // Snap the volume to whole texels so the shadow doesn't shimmer
  const float r = sphere.radius;
  light.texel_size = 2.0F * r / static_cast<float>(input.resolution);
  const float x = std::round(center.x / light.texel_size) * light.texel_size;
  const float y = std::round(center.y / light.texel_size) * light.texel_size;

  // Right-handed views look along -Z
  const float depth =
      handedness == bx::Handedness::Right ? -center.z : center.z;
  bx::mtxOrtho(light.proj.data(), x - r, x + r, y - r, y + r,
               depth - r - input.caster_margin, depth + r, 0.0F,
               homogeneous_depth, handedness);
  return light;
}

auto ShadowTextureMatrix(const LightMatrices& light, bool homogeneous_depth,
                         bool origin_bottom_left) -> std::array<float, 16> {
  const float sy = origin_bottom_left ? 0.5F : -0.5F;
  const float sz = homogeneous_depth ? 0.5F : 1.0F;
  const float tz = homogeneous_depth ? 0.5F : 0.0F;
  // NDC to texture space: x and y from -1..1 to UV 0..1, depth to 0..1
  const std::array<float, 16> crop{
      0.5F, 0.0F, 0.0F, 0.0F,  //
      0.0F, sy,   0.0F, 0.0F,  //
      0.0F, 0.0F, sz,   0.0F,  //
      0.5F, 0.5F, tz,   1.0F,
  };

  std::array<float, 16> view_proj{};
  std::array<float, 16> result{};
  bx::mtxMul(view_proj.data(), light.view.data(), light.proj.data());
  bx::mtxMul(result.data(), view_proj.data(), crop.data());
  return result;
}

}  // namespace engine

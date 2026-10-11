// Directional shadow fitting: the light volume must contain the camera's
// slice, keep its size as the camera turns, and keep its texel grid fixed in
// the world as the camera moves.

#include "engine/math/shadow_fit.h"

#include <bx/math.h>
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "engine/math/rotation.h"

namespace engine {

namespace {

constexpr float kTolerance = 1e-4F;
constexpr auto kHandedness = bx::Handedness::Right;

// Straight down, steep, low sun, and a hair off vertical
constexpr std::array<bx::Vec3, 4> kLights{{
    {0.0F, 1.0F, 0.0F},
    {-0.4F, 1.0F, 0.3F},
    {0.9F, 0.15F, -0.2F},
    {0.0F, 1.0F, 1e-4F},
}};

constexpr std::array<YawPitch, 4> kLooks{{
    {.yaw = 0.0F, .pitch = 0.0F},
    {.yaw = 1.2F, .pitch = -0.6F},
    {.yaw = -2.5F, .pitch = 0.4F},
    {.yaw = 0.3F, .pitch = -1.4F},
}};

/** @brief A 16:9 camera at @p eye looking along @p look, lit by @p light. */
auto Input(bx::Vec3 eye, YawPitch look, bx::Vec3 light) -> ShadowFitInput {
  return {.eye = eye,
          .rotation = YawPitchToQuat(look),
          .aspect = 16.0F / 9.0F,
          .light_dir = light};
}

/** @brief The eight corners of the input's slice, in world space. */
auto SliceCorners(const ShadowFitInput& in) -> std::vector<bx::Vec3> {
  const bx::Vec3 forward = bx::mul(bx::Vec3{0.0F, 0.0F, -1.0F}, in.rotation);
  const bx::Vec3 up = bx::mul(bx::Vec3{0.0F, 1.0F, 0.0F}, in.rotation);
  const bx::Vec3 right = bx::mul(bx::Vec3{1.0F, 0.0F, 0.0F}, in.rotation);
  const float tan_half = std::tan(bx::toRad(in.fov_degrees) * 0.5F);

  std::vector<bx::Vec3> corners;
  for (const float d : {in.slice_near, in.slice_far}) {
    for (const float sx : {-1.0F, 1.0F}) {
      for (const float sy : {-1.0F, 1.0F}) {
        const bx::Vec3 offset =
            bx::add(bx::mul(right, sx * d * tan_half * in.aspect),
                    bx::mul(up, sy * d * tan_half));
        corners.push_back(
            bx::add(in.eye, bx::add(bx::mul(forward, d), offset)));
      }
    }
  }
  return corners;
}

/** @brief Where @p point lands in the light's NDC. */
auto ToLightNdc(const LightMatrices& light, const bx::Vec3& point) -> bx::Vec3 {
  std::array<float, 16> view_proj{};
  bx::mtxMul(view_proj.data(), light.view.data(), light.proj.data());
  return bx::mulH(point, view_proj.data());
}

/** @brief Expects @p ndc inside the clip volume, within kTolerance. */
void ExpectInsideClip(const bx::Vec3& ndc, bool homogeneous_depth) {
  const float z_min = homogeneous_depth ? -1.0F : 0.0F;
  EXPECT_GE(ndc.x, -1.0F - kTolerance);
  EXPECT_LE(ndc.x, 1.0F + kTolerance);
  EXPECT_GE(ndc.y, -1.0F - kTolerance);
  EXPECT_LE(ndc.y, 1.0F + kTolerance);
  EXPECT_GE(ndc.z, z_min - kTolerance);
  EXPECT_LE(ndc.z, 1.0F + kTolerance);
}

TEST(ShadowFit, SliceAndMarginInsideLightVolume) {
  for (const bool homogeneous : {false, true}) {
    for (const bx::Vec3 light_dir : kLights) {
      for (const YawPitch look : kLooks) {
        const ShadowFitInput in = Input({5.0F, 2.0F, -3.0F}, look, light_dir);
        const LightMatrices light =
            FitDirectionalShadow(in, homogeneous, kHandedness);
        const bx::Vec3 to_light = bx::normalize(light_dir);
        for (const bx::Vec3& corner : SliceCorners(in)) {
          ExpectInsideClip(ToLightNdc(light, corner), homogeneous);
          // A caster the margin's distance toward the light still lands
          const bx::Vec3 caster =
              bx::add(corner, bx::mul(to_light, in.caster_margin));
          ExpectInsideClip(ToLightNdc(light, caster), homogeneous);
        }
      }
    }
  }
}

TEST(ShadowFit, SizeIgnoresCameraPose) {
  const bx::Vec3 light_dir = kLights.at(1);
  const LightMatrices reference = FitDirectionalShadow(
      Input({0.0F, 0.0F, 0.0F}, kLooks.at(0), light_dir), false, kHandedness);
  for (const YawPitch look : kLooks) {
    const LightMatrices light = FitDirectionalShadow(
        Input({12.0F, 3.0F, -7.5F}, look, light_dir), false, kHandedness);
    EXPECT_FLOAT_EQ(light.proj.at(0), reference.proj.at(0));
    EXPECT_FLOAT_EQ(light.proj.at(5), reference.proj.at(5));
    EXPECT_FLOAT_EQ(light.proj.at(10), reference.proj.at(10));
    EXPECT_FLOAT_EQ(light.texel_size, reference.texel_size);
  }
}

TEST(ShadowFit, TexelGridFixedInWorld) {
  const bx::Vec3 point{0.3F, 0.0F, 0.7F};
  const bx::Vec3 light_dir = kLights.at(1);

  // Texel coordinates of a fixed point, seen with the camera at @p eye
  const auto texel_of = [&](bx::Vec3 eye) -> std::array<float, 2> {
    const ShadowFitInput in = Input(eye, kLooks.at(1), light_dir);
    const LightMatrices light = FitDirectionalShadow(in, false, kHandedness);
    const bx::Vec3 ndc = ToLightNdc(light, point);
    const auto res = static_cast<float>(in.resolution);
    return {((ndc.x * 0.5F) + 0.5F) * res, ((ndc.y * 0.5F) + 0.5F) * res};
  };

  // Sub-texel and multi-texel moves alike shift the point by whole texels
  const std::array<float, 2> reference = texel_of({0.0F, 0.0F, 0.0F});
  for (const bx::Vec3 eye :
       {bx::Vec3{0.013F, 0.0F, 0.0F}, bx::Vec3{0.0F, 0.021F, -0.017F},
        bx::Vec3{3.7F, 1.1F, -2.9F}}) {
    const std::array<float, 2> texel = texel_of(eye);
    for (std::size_t axis = 0; axis < 2; ++axis) {
      const float shift = texel.at(axis) - reference.at(axis);
      EXPECT_NEAR(shift, std::round(shift), 1e-2F) << "axis " << axis;
    }
  }
}

TEST(ShadowFit, VerticalLightIsFinite) {
  for (const bx::Vec3 light_dir :
       {bx::Vec3{0.0F, 1.0F, 0.0F}, bx::Vec3{0.0F, -1.0F, 0.0F}}) {
    const LightMatrices light = FitDirectionalShadow(
        Input({1.0F, 2.0F, 3.0F}, kLooks.at(1), light_dir), false, kHandedness);
    for (std::size_t i = 0; i < 16; ++i) {
      EXPECT_TRUE(std::isfinite(light.view.at(i))) << "view " << i;
      EXPECT_TRUE(std::isfinite(light.proj.at(i))) << "proj " << i;
    }
  }
}

TEST(ShadowFit, TextureMatrixMapsNdcToUvAndDepth) {
  for (const bool homogeneous : {false, true}) {
    for (const bool bottom_left : {false, true}) {
      const ShadowFitInput in =
          Input({5.0F, 2.0F, -3.0F}, kLooks.at(2), kLights.at(1));
      const LightMatrices light =
          FitDirectionalShadow(in, homogeneous, kHandedness);
      const auto to_texture =
          ShadowTextureMatrix(light, homogeneous, bottom_left);

      for (const bx::Vec3& corner : SliceCorners(in)) {
        const bx::Vec3 ndc = ToLightNdc(light, corner);
        const bx::Vec3 uv = bx::mulH(corner, to_texture.data());
        EXPECT_NEAR(uv.x, (ndc.x * 0.5F) + 0.5F, kTolerance);
        // Top-left origins put NDC +Y at V = 0
        const float v_sign = bottom_left ? 0.5F : -0.5F;
        EXPECT_NEAR(uv.y, (ndc.y * v_sign) + 0.5F, kTolerance);
        const float depth = homogeneous ? (ndc.z * 0.5F) + 0.5F : ndc.z;
        EXPECT_NEAR(uv.z, depth, kTolerance);
        EXPECT_GE(uv.z, -kTolerance);
        EXPECT_LE(uv.z, 1.0F + kTolerance);
      }
    }
  }
}

}  // namespace

}  // namespace engine

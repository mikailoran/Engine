// Camera convention: a Transform's rotation is the pose, and the derived
// matrices must match bx's lookAt and projection for the same view.

#include "engine/ecs/components/camera.h"

#include <bx/bounds.h>
#include <bx/math.h>
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>

#include "engine/ecs/components/transform.h"
#include "engine/math/rotation.h"
#include "engine/platform/screen.h"

namespace {

constexpr float kTolerance = 1e-4F;

// Stops short of +-90 degrees pitch, where lookAt's up vector degenerates
constexpr std::array<float, 5> kYaws{-3.0F, -1.5F, 0.0F, 0.7F, 2.5F};
constexpr std::array<float, 5> kPitches{-1.2F, -0.3F, 0.0F, 0.4F, 1.2F};

/** @brief Direction for @p yaw and @p pitch, as YawPitchToQuat documents. */
auto Direction(float yaw, float pitch) -> bx::Vec3 {
  return {std::cos(pitch) * std::sin(yaw), std::sin(pitch),
          std::cos(pitch) * std::cos(yaw)};
}

/** @brief Expects two vectors to match within kTolerance. */
void ExpectNear(const bx::Vec3& actual, const bx::Vec3& expected) {
  EXPECT_NEAR(actual.x, expected.x, kTolerance);
  EXPECT_NEAR(actual.y, expected.y, kTolerance);
  EXPECT_NEAR(actual.z, expected.z, kTolerance);
}

TEST(Camera, YawPitchPointsForwardAndKeepsRightLevel) {
  for (const float yaw : kYaws) {
    for (const float pitch : kPitches) {
      const bx::Quaternion q = YawPitchToQuat({.yaw = yaw, .pitch = pitch});
      ExpectNear(bx::mul(bx::Vec3{0.0F, 0.0F, 1.0F}, q), Direction(yaw, pitch));
      EXPECT_NEAR(bx::mul(bx::Vec3{1.0F, 0.0F, 0.0F}, q).y, 0.0F, kTolerance);
    }
  }
}

TEST(Camera, ViewMatrixMatchesLookAt) {
  const bx::Vec3 eye{3.0F, 1.5F, -7.0F};
  for (const float yaw : kYaws) {
    for (const float pitch : kPitches) {
      const Transform pose{
          .position = eye,
          .rotation = YawPitchToQuat({.yaw = yaw, .pitch = pitch})};
      std::array<float, 16> expected{};
      bx::mtxLookAt(expected.data(), eye, bx::add(eye, Direction(yaw, pitch)));

      const auto view = ViewMatrix(pose);
      for (std::size_t i = 0; i < view.size(); ++i) {
        EXPECT_NEAR(view.at(i), expected.at(i), kTolerance) << "element " << i;
      }
    }
  }
}

TEST(Camera, ProjectionMatrixMatchesMtxProj) {
  const Camera lens{
      .fov_degrees = 70.0F, .near_plane = 0.2F, .far_plane = 250.0F};
  for (const bool homogeneous_depth : {false, true}) {
    std::array<float, 16> expected{};
    bx::mtxProj(expected.data(), lens.fov_degrees, 1.5F, lens.near_plane,
                lens.far_plane, homogeneous_depth);
    EXPECT_EQ(ProjectionMatrix(lens, 1.5F, homogeneous_depth), expected);
  }
}

// makeRay unprojects depth 0 as the near plane, whatever the backend's range
TEST(Camera, CenterRayStartsOnNearPlaneAlongForward) {
  const Camera lens{};
  const ScreenSize size{.width = 1280.0F, .height = 720.0F};
  for (const float yaw : kYaws) {
    for (const float pitch : kPitches) {
      const Transform pose{
          .position = {1.0F, 2.0F, 3.0F},
          .rotation = YawPitchToQuat({.yaw = yaw, .pitch = pitch})};
      const bx::Ray ray = ScreenPositionToRay(
          lens, pose, {.x = size.width / 2.0F, .y = size.height / 2.0F}, size);

      const bx::Vec3 forward = Direction(yaw, pitch);
      ExpectNear(ray.dir, forward);
      ExpectNear(ray.pos, bx::mad(forward, lens.near_plane, pose.position));
    }
  }
}

}  // namespace

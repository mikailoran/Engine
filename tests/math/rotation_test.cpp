// Rotation convention: quaternions in Transform must draw exactly what the old
// Euler angles drew, and convert back to Euler angles without changing it.

#include "math/rotation.h"

#include <bx/constants.h>
#include <bx/math.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <random>

#include "ecs/components/transform.h"

namespace {

constexpr int kSamples = 10000;

/** @brief Draws uniform floats from [min, max) with a fixed seed. */
class Random {
 public:
  /** @brief Returns the next sample in [min, max). */
  auto Next(float min, float max) -> float {
    return std::uniform_real_distribution<float>(min, max)(engine_);
  }

  /** @brief Returns a vector with each component in [min, max). */
  auto NextVec3(float min, float max) -> bx::Vec3 {
    return {Next(min, max), Next(min, max), Next(min, max)};
  }

 private:
  // A fixed seed keeps failures reproducible
  // NOLINTNEXTLINE(bugprone-random-generator-seed)
  std::mt19937 engine_{42};
};

/** @brief Largest component difference between two vectors. */
auto MaxDiff(const bx::Vec3& a, const bx::Vec3& b) -> float {
  return std::max(
      {std::abs(a.x - b.x), std::abs(a.y - b.y), std::abs(a.z - b.z)});
}

/** @brief Largest difference in where two rotations send the basis axes. */
auto RotationDiff(const bx::Quaternion& a, const bx::Quaternion& b) -> float {
  float diff = 0.0F;
  for (const bx::Vec3 axis :
       {bx::Vec3{1.0F, 0.0F, 0.0F}, bx::Vec3{0.0F, 1.0F, 0.0F},
        bx::Vec3{0.0F, 0.0F, 1.0F}}) {
    diff = std::max(diff, MaxDiff(bx::mul(axis, a), bx::mul(axis, b)));
  }
  return diff;
}

/** @brief A random unit quaternion. */
auto RandomRotation(Random& random) -> bx::Quaternion {
  return bx::normalize(
      bx::Quaternion{random.Next(-1.0F, 1.0F), random.Next(-1.0F, 1.0F),
                     random.Next(-1.0F, 1.0F), random.Next(-1.0F, 1.0F)});
}

TEST(Rotation, ZeroAnglesGiveIdentity) {
  const bx::Quaternion q = EulerToQuat({0.0F, 0.0F, 0.0F});
  EXPECT_FLOAT_EQ(q.w, 1.0F);
  EXPECT_FLOAT_EQ(bx::length(bx::Vec3{q.x, q.y, q.z}), 0.0F);
}

// Scene files and old Transforms used mtxSRT: drawing must not change
TEST(Rotation, ModelMatrixMatchesMtxSrt) {
  Random random;
  float max_diff = 0.0F;
  for (int i = 0; i < kSamples; ++i) {
    const bx::Vec3 euler = random.NextVec3(-bx::kPi, bx::kPi);
    const Transform transform{.position = random.NextVec3(-10.0F, 10.0F),
                              .rotation = EulerToQuat(euler),
                              .scale = random.NextVec3(0.1F, 5.0F)};
    std::array<float, 16> expected{};
    bx::mtxSRT(expected.data(), transform.scale.x, transform.scale.y,
               transform.scale.z, euler.x, euler.y, euler.z,
               transform.position.x, transform.position.y,
               transform.position.z);
    const std::array<float, 16> actual = ModelMatrix(transform);
    for (std::size_t j = 0; j < actual.size(); ++j) {
      max_diff = std::max(max_diff, std::abs(actual.at(j) - expected.at(j)));
    }
  }
  EXPECT_LT(max_diff, 1e-4F);
}

// The renderer must apply q the same way bx::mul(Vec3, Quaternion) and Jolt do
TEST(Rotation, ModelMatrixAppliesQuaternionRotation) {
  Random random;
  float max_diff = 0.0F;
  for (int i = 0; i < kSamples; ++i) {
    const bx::Quaternion q = RandomRotation(random);
    const std::array<float, 16> mtx = ModelMatrix(Transform{.rotation = q});
    const bx::Vec3 v = random.NextVec3(-1.0F, 1.0F);
    max_diff =
        std::max(max_diff, MaxDiff(bx::mul(v, mtx.data()), bx::mul(v, q)));
  }
  EXPECT_LT(max_diff, 1e-5F);
}

// Inspector round trip: angles in range come back unchanged
TEST(Rotation, QuatToEulerReturnsTheSameAngles) {
  Random random;
  float max_diff = 0.0F;
  for (int i = 0; i < kSamples; ++i) {
    const bx::Vec3 euler{random.Next(-3.1F, 3.1F), random.Next(-1.5F, 1.5F),
                         random.Next(-3.1F, 3.1F)};
    max_diff =
        std::max(max_diff, MaxDiff(QuatToEuler(EulerToQuat(euler)), euler));
  }
  EXPECT_LT(max_diff, 1e-3F);
}

// Any rotation, e.g. a tumbling body's, survives the trip through Euler
TEST(Rotation, QuatToEulerKeepsAnyRotation) {
  Random random;
  float max_diff = 0.0F;
  for (int i = 0; i < kSamples; ++i) {
    const bx::Quaternion q = RandomRotation(random);
    max_diff = std::max(max_diff, RotationDiff(EulerToQuat(QuatToEuler(q)), q));
  }
  EXPECT_LT(max_diff, 1e-4F);
}

// At pitch +-90 degrees only X + Z is defined; the rotation must still hold
TEST(Rotation, QuatToEulerKeepsRotationAtGimbalLock) {
  Random random;
  float max_diff = 0.0F;
  for (const float pitch : {bx::kPiHalf, -bx::kPiHalf}) {
    for (int i = 0; i < kSamples; ++i) {
      const bx::Quaternion q = EulerToQuat(
          {random.Next(-3.1F, 3.1F), pitch, random.Next(-3.1F, 3.1F)});
      max_diff =
          std::max(max_diff, RotationDiff(EulerToQuat(QuatToEuler(q)), q));
    }
  }
  EXPECT_LT(max_diff, 2e-3F);
}

}  // namespace

// Splitting meshes into per-bone point sets, the input to hitbox hulls.

#include "engine/resource/bone_parts.h"

#include <bx/math.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <vector>

#include "engine/resource/cpu_mesh.h"
#include "engine/resource/gltf_reader.h"

namespace engine {

namespace {

/** @brief The mean of @p points. @pre Not empty. */
auto Centroid(const std::vector<bx::Vec3>& points) -> bx::Vec3 {
  bx::Vec3 sum{0.0F};
  for (const bx::Vec3& point : points) {
    sum = bx::add(sum, point);
  }
  return bx::mul(sum, 1.0F / static_cast<float>(points.size()));
}

TEST(BoneParts, GroupsPositionsByBoneInBoneOrder) {
  CpuMesh mesh;
  mesh.positions = {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {2.0F, 0.0F, 0.0F}};
  mesh.bones = {3, 1, 3};

  const auto parts = SplitByBone(mesh);

  ASSERT_EQ(parts.size(), 2U);
  EXPECT_EQ(parts.at(0).bone, 1U);
  ASSERT_EQ(parts.at(0).points.size(), 1U);
  EXPECT_FLOAT_EQ(parts.at(0).points.at(0).x, 1.0F);
  EXPECT_EQ(parts.at(1).bone, 3U);
  ASSERT_EQ(parts.at(1).points.size(), 2U);
  EXPECT_FLOAT_EQ(parts.at(1).points.at(0).x, 0.0F);
  EXPECT_FLOAT_EQ(parts.at(1).points.at(1).x, 2.0F);
}

TEST(BoneParts, SplitsATPoseIntoBothArms) {
  // Arms reach about 0.9 m out on either side
  const CpuMesh mesh = ReadGltf(std::filesystem::path(ENGINE_ASSET_DIR) /
                                "meshes/characters/Male_Casual.glb");

  const auto parts = SplitByBone(mesh);

  std::size_t points = 0;
  bool left = false;
  bool right = false;
  for (const BonePart& part : parts) {
    ASSERT_FALSE(part.points.empty());
    points += part.points.size();
    const bx::Vec3 centre = Centroid(part.points);
    left = left || centre.x > 0.5F;
    right = right || centre.x < -0.5F;
  }
  EXPECT_EQ(points, mesh.positions.size());
  EXPECT_TRUE(left);
  EXPECT_TRUE(right);
}

}  // namespace

}  // namespace engine

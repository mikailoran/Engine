#include "engine/resource/skeleton.h"

#include <bx/math.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

#include "engine/ecs/components/transform.h"
#include "engine/resource/cpu_mesh.h"
#include "engine/resource/gltf_reader.h"

namespace engine {

namespace {

/** @brief Reads a mesh from the source tree's assets/. */
auto Read(const char* relative) -> CpuMesh {
  return ReadGltf(std::filesystem::path(ENGINE_ASSET_DIR) / relative);
}

/** @brief Index of the joint named @p name; fails the test if absent. */
auto JointIndex(const Skeleton& skeleton, const std::string& name)
    -> std::size_t {
  const auto it = std::ranges::find(skeleton.joints, name, &Joint::name);
  EXPECT_NE(it, skeleton.joints.end()) << "no joint " << name;
  return static_cast<std::size_t>(it - skeleton.joints.begin());
}

/** @brief Every joint's model-space matrix in rest pose. */
auto RestModelMatrices(const Skeleton& skeleton) -> std::vector<Mat4> {
  std::vector<Mat4> model(skeleton.joints.size());
  std::vector<bool> done(skeleton.joints.size(), false);
  for (std::size_t i = 0; i < skeleton.joints.size(); ++i) {
    // Parents may follow their children: resolve the chain from its top
    std::vector<std::size_t> chain{i};
    for (auto parent = skeleton.joints.at(i).parent;
         parent && !done.at(chain.back());
         parent = skeleton.joints.at(*parent).parent) {
      chain.push_back(*parent);
    }
    for (const std::size_t index : std::views::reverse(chain)) {
      if (done.at(index)) {
        continue;
      }
      const Joint& joint = skeleton.joints.at(index);
      const auto local = ModelMatrix({.position = joint.translation,
                                      .rotation = joint.rotation,
                                      .scale = joint.scale});
      const Mat4& parent =
          joint.parent ? model.at(*joint.parent) : skeleton.root_transform;
      // Row vectors: the local transform applies first
      bx::mtxMul(model.at(index).data(), local.data(), parent.data());
      done.at(index) = true;
    }
  }
  return model;
}

/** @brief The skeleton of a mesh under assets/; empty if it has none. */
auto SkeletonOf(const char* relative) -> Skeleton {
  return Read(relative).skeleton.value_or(Skeleton{});
}

/** @brief Checks parents are in range and every chain ends at a root. */
void ExpectTree(const Skeleton& skeleton) {
  for (const Joint& joint : skeleton.joints) {
    std::optional<std::uint32_t> parent = joint.parent;
    std::size_t steps = 0;
    while (parent) {
      ASSERT_LT(*parent, skeleton.joints.size());
      ASSERT_LE(++steps, skeleton.joints.size()) << "cycle at " << joint.name;
      parent = skeleton.joints.at(*parent).parent;
    }
  }
}

TEST(Skeleton, MaleCasualHasOneTreeOf31Joints) {
  const Skeleton skeleton = SkeletonOf("meshes/characters/Male_Casual.glb");

  EXPECT_EQ(skeleton.joints.size(), 31U);
  EXPECT_EQ(std::ranges::count_if(
                skeleton.joints,
                [](const Joint& joint) -> bool { return !joint.parent; }),
            1);
  ExpectTree(skeleton);
  JointIndex(skeleton, "Hips");
  JointIndex(skeleton, "Head");
}

TEST(Skeleton, SuperheroUsesTheUalRig) {
  const Skeleton skeleton = SkeletonOf("meshes/characters/Superhero_Male.glb");

  ASSERT_EQ(skeleton.joints.size(), 65U);
  ExpectTree(skeleton);
  const auto root = JointIndex(skeleton, "root");
  EXPECT_FALSE(skeleton.joints.at(root).parent);
  const auto hand = JointIndex(skeleton, "hand_r");
  EXPECT_EQ(skeleton.joints.at(hand).parent,
            JointIndex(skeleton, "lowerarm_r"));
}

// Inverse bind matrices undo the rest pose, so it must be the bind pose
TEST(Skeleton, RestPoseIsTheBindPose) {
  for (const char* path :
       {"meshes/characters/Male_Casual.glb",
        "meshes/characters/Superhero_Male.glb", "animations/UAL1.glb"}) {
    SCOPED_TRACE(path);
    const Skeleton skeleton = SkeletonOf(path);
    ASSERT_FALSE(skeleton.joints.empty());
    const auto model = RestModelMatrices(skeleton);
    for (std::size_t i = 0; i < model.size(); ++i) {
      const Joint& joint = skeleton.joints.at(i);
      Mat4 product{};
      bx::mtxMul(product.data(), joint.inverse_bind.data(), model.at(i).data());
      for (std::size_t k = 0; k < product.size(); ++k) {
        const float identity = (k % 5 == 0) ? 1.0F : 0.0F;
        ASSERT_NEAR(product.at(k), identity, 1e-3F)
            << joint.name << " [" << k << "]";
      }
    }
  }
}

TEST(Skeleton, StaticMeshHasNone) {
  EXPECT_FALSE(Read("meshes/cube.glb").skeleton);
  EXPECT_FALSE(Read("levels/level02.glb").skeleton);
}

}  // namespace

}  // namespace engine

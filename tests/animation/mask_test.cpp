#include "engine/animation/mask.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>

#include "engine/resource/gltf_reader.h"
#include "engine/resource/skeleton.h"

namespace engine::animation {

namespace {

/** @brief The Superhero's skeleton, the UAL rig. */
auto Hero() -> Skeleton {
  return ReadGltfClips(std::filesystem::path(ENGINE_ASSET_DIR) /
                       "meshes/characters/Superhero_Male.glb")
      .skeleton;
}

/** @brief Index of the joint named @p name, or the joint count if absent. */
auto JointIndex(const Skeleton& skeleton, const std::string& name)
    -> std::size_t {
  return static_cast<std::size_t>(
      std::ranges::find(skeleton.joints, name, &Joint::name) -
      skeleton.joints.begin());
}

TEST(JointMask, CoversTheSubtreeOnly) {
  const Skeleton hero = Hero();
  const auto mask = JointMask(hero, "spine_01");
  ASSERT_EQ(mask.size(), hero.joints.size());
  for (const char* covered :
       {"spine_01", "spine_03", "Head", "hand_r", "thumb_03_l"}) {
    EXPECT_FLOAT_EQ(mask.at(JointIndex(hero, covered)), 1.0F) << covered;
  }
  for (const char* outside : {"root", "pelvis", "thigh_l", "foot_r"}) {
    EXPECT_FLOAT_EQ(mask.at(JointIndex(hero, outside)), 0.0F) << outside;
  }
}

TEST(JointMask, UnknownRootCoversNothing) {
  const auto mask = JointMask(Hero(), "no_such_joint");
  EXPECT_TRUE(
      std::ranges::all_of(mask, [](float w) -> bool { return w == 0.0F; }));
}

}  // namespace

}  // namespace engine::animation

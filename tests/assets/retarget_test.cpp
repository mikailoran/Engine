#include "engine/resource/retarget.h"

#include <bx/math.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "engine/resource/animation_clip.h"
#include "engine/resource/gltf_reader.h"
#include "engine/resource/skeleton.h"

namespace engine {

namespace {

/** @brief Path of a file under the source tree's assets/. */
auto Asset(const char* relative) -> std::filesystem::path {
  return std::filesystem::path(ENGINE_ASSET_DIR) / relative;
}

/** @brief The clip named @p name in @p file, or {} if absent. */
auto Find(const ClipFile& file, const std::string& name) -> AnimationClip {
  const auto it = std::ranges::find(file.clips, name, &AnimationClip::name);
  return it != file.clips.end() ? *it : AnimationClip{};
}

/** @brief Index of the joint named @p name, or the joint count if absent. */
auto JointIndex(const Skeleton& skeleton, const std::string& name)
    -> std::size_t {
  return static_cast<std::size_t>(
      std::ranges::find(skeleton.joints, name, &Joint::name) -
      skeleton.joints.begin());
}

/** @brief The track of joint @p joint in @p clip, or {} if it has none. */
auto TrackOf(const AnimationClip& clip, std::size_t joint) -> JointTrack {
  const auto it = std::ranges::find(clip.tracks, joint, &JointTrack::joint);
  return it != clip.tracks.end() ? *it : JointTrack{};
}

/** @brief UAL1's clips and the Superhero's skeleton, the shipped pairing. */
class UalOnSuperhero : public testing::Test {
 protected:
  ClipFile ual_ = ReadGltfClips(Asset("animations/UAL1.glb"));
  Skeleton hero_ =
      ReadGltfClips(Asset("meshes/characters/Superhero_Male.glb")).skeleton;
};

TEST_F(UalOnSuperhero, KeepsRotationsAndTheTargetsBoneLengths) {
  const AnimationClip walk = Find(ual_, "Walk_Loop");
  const AnimationClip mapped = RetargetClip(walk, ual_.skeleton, hero_);
  ASSERT_EQ(mapped.tracks.size(), hero_.joints.size());
  EXPECT_FLOAT_EQ(mapped.duration, walk.duration);

  const auto source_arm =
      TrackOf(walk, JointIndex(ual_.skeleton, "upperarm_r"));
  const auto arm = TrackOf(mapped, JointIndex(hero_, "upperarm_r"));
  ASSERT_EQ(arm.rotations.size(), source_arm.rotations.size());
  ASSERT_FALSE(arm.rotations.empty());
  EXPECT_FLOAT_EQ(arm.rotations.front().value.x,
                  source_arm.rotations.front().value.x);
  // Limbs hold the target's rest translation and scale
  EXPECT_TRUE(arm.translations.empty());
  EXPECT_TRUE(arm.scales.empty());
  EXPECT_TRUE(
      TrackOf(mapped, JointIndex(hero_, "calf_l")).translations.empty());
}

TEST_F(UalOnSuperhero, ScalesTheHipsByRestLength) {
  const AnimationClip walk = Find(ual_, "Walk_Loop");
  const AnimationClip mapped = RetargetClip(walk, ual_.skeleton, hero_);

  const std::size_t source_pelvis = JointIndex(ual_.skeleton, "pelvis");
  const std::size_t pelvis = JointIndex(hero_, "pelvis");
  const float ratio =
      bx::length(hero_.joints.at(pelvis).translation) /
      bx::length(ual_.skeleton.joints.at(source_pelvis).translation);
  EXPECT_NEAR(ratio, 1.035F, 0.005F);

  const auto from = TrackOf(walk, source_pelvis).translations;
  const auto to = TrackOf(mapped, pelvis).translations;
  ASSERT_EQ(to.size(), from.size());
  ASSERT_FALSE(to.empty());
  EXPECT_NEAR(to.front().value.z, from.front().value.z * ratio, 1e-5F);
  // The root keeps its own keys
  EXPECT_FALSE(TrackOf(mapped, JointIndex(hero_, "root")).translations.empty());
}

TEST(Retarget, SameRigIsUnchanged) {
  const ClipFile file =
      ReadGltfClips(Asset("meshes/characters/Male_Casual.glb"));
  const AnimationClip& walk = file.clips.front();
  const AnimationClip mapped = RetargetClip(walk, file.skeleton, file.skeleton);
  ASSERT_EQ(mapped.tracks.size(), walk.tracks.size());
  for (std::size_t i = 0; i < walk.tracks.size(); ++i) {
    EXPECT_EQ(mapped.tracks.at(i).joint, walk.tracks.at(i).joint);
    EXPECT_EQ(mapped.tracks.at(i).translations.size(),
              walk.tracks.at(i).translations.size());
    EXPECT_EQ(mapped.tracks.at(i).scales.size(),
              walk.tracks.at(i).scales.size());
  }
}

TEST(Retarget, NoSharedJointNameThrows) {
  const ClipFile file = ReadGltfClips(Asset("animations/UAL1.glb"));
  Skeleton renamed = file.skeleton;
  for (Joint& joint : renamed.joints) {
    joint.name += "_other";
  }
  EXPECT_THROW(RetargetClip(file.clips.front(), file.skeleton, renamed),
               std::runtime_error);
}

}  // namespace

}  // namespace engine

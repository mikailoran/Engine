#include "engine/resource/animation_clip.h"

#include <bx/math.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/resource/gltf_reader.h"

namespace engine {

namespace {

/** @brief Reads the clips of a file under the source tree's assets/. */
auto Clips(const char* relative) -> ClipFile {
  return ReadGltfClips(std::filesystem::path(ENGINE_ASSET_DIR) / relative);
}

/** @brief The clip named @p name; fails the test and returns {} if absent. */
auto Find(const ClipFile& file, const std::string& name) -> AnimationClip {
  const auto it = std::ranges::find(file.clips, name, &AnimationClip::name);
  EXPECT_NE(it, file.clips.end()) << "no clip " << name;
  return it != file.clips.end() ? *it : AnimationClip{};
}

/** @brief Checks key times increase strictly and stay within the clip. */
template <class Key>
void ExpectKeyTimes(const std::vector<Key>& keys, float duration) {
  for (std::size_t i = 0; i < keys.size(); ++i) {
    ASSERT_GE(keys.at(i).time, 0.0F);
    ASSERT_LE(keys.at(i).time, duration);
    if (i > 0) {
      ASSERT_GT(keys.at(i).time, keys.at(i - 1).time);
    }
  }
}

/** @brief Checks the invariants AnimationClip and JointTrack document. */
void ExpectWellFormed(const ClipFile& file) {
  for (const AnimationClip& clip : file.clips) {
    SCOPED_TRACE(clip.name);
    EXPECT_GT(clip.duration, 0.0F);
    for (std::size_t i = 0; i < clip.tracks.size(); ++i) {
      const JointTrack& track = clip.tracks.at(i);
      ASSERT_LT(track.joint, file.skeleton.joints.size());
      if (i > 0) {
        ASSERT_GT(track.joint, clip.tracks.at(i - 1).joint);
      }
      ExpectKeyTimes(track.translations, clip.duration);
      ExpectKeyTimes(track.rotations, clip.duration);
      ExpectKeyTimes(track.scales, clip.duration);
      for (const RotationKey& key : track.rotations) {
        ASSERT_NEAR(bx::dot(key.value, key.value), 1.0F, 1e-4F);
      }
    }
  }
}

TEST(AnimationClip, ReadsMaleCasualsClips) {
  const ClipFile file = Clips("meshes/characters/Male_Casual.glb");
  EXPECT_EQ(file.skeleton.joints.size(), 31U);
  ASSERT_EQ(file.clips.size(), 12U);
  ExpectWellFormed(file);

  const AnimationClip walk = Find(file, "Man_Walk");
  const bool rotates = std::ranges::any_of(
      walk.tracks,
      [](const JointTrack& track) -> bool { return !track.rotations.empty(); });
  EXPECT_TRUE(rotates);
  Find(file, "Man_Death");
  Find(file, "Man_Idle");
}

TEST(AnimationClip, ReadsUalClipsOnEveryJoint) {
  const ClipFile file = Clips("animations/UAL1.glb");
  ASSERT_EQ(file.skeleton.joints.size(), 65U);
  EXPECT_EQ(file.clips.size(), 13U);
  ExpectWellFormed(file);

  const AnimationClip walk = Find(file, "Walk_Loop");
  EXPECT_NEAR(walk.duration, 1.333F, 1e-3F);
  ASSERT_EQ(walk.tracks.size(), 65U);
  for (const JointTrack& track : walk.tracks) {
    EXPECT_FALSE(track.translations.empty());
    EXPECT_FALSE(track.rotations.empty());
    EXPECT_FALSE(track.scales.empty());
  }
  Find(file, "Pistol_Shoot");
}

TEST(AnimationClip, FileWithoutSkinThrows) {
  EXPECT_THROW(Clips("meshes/cube.glb"), std::runtime_error);
}

}  // namespace

}  // namespace engine

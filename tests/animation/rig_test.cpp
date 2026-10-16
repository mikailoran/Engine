#include "engine/animation/rig.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <vector>

#include "engine/resource/animation_clip.h"
#include "engine/resource/gltf_reader.h"
#include "engine/resource/retarget.h"
#include "engine/resource/skeleton.h"

namespace engine::animation {

namespace {

/** @brief Path of a file under the source tree's assets/. */
auto Asset(const char* relative) -> std::filesystem::path {
  return std::filesystem::path(ENGINE_ASSET_DIR) / relative;
}

TEST(Rig, BuildsEveryClipOnItsOwnRig) {
  const ClipFile file =
      ReadGltfClips(Asset("meshes/characters/Male_Casual.glb"));
  const Rig rig(file.skeleton);
  EXPECT_EQ(rig.JointCount(), 31U);
  for (const AnimationClip& source : file.clips) {
    const Clip clip(source, rig);
    EXPECT_EQ(clip.Name(), source.name);
    EXPECT_NEAR(clip.Duration(), source.duration, 1e-5F);
  }
}

TEST(Rig, BuildsUalClipsOnTheSuperhero) {
  const ClipFile ual = ReadGltfClips(Asset("animations/UAL1.glb"));
  const Skeleton hero =
      ReadGltfClips(Asset("meshes/characters/Superhero_Male.glb")).skeleton;
  const Rig rig(hero);
  EXPECT_EQ(rig.JointCount(), 65U);
  EXPECT_EQ(rig.Source().joints.size(), hero.joints.size());
  std::vector<Clip> clips;
  clips.reserve(ual.clips.size());
  for (const AnimationClip& source : ual.clips) {
    clips.emplace_back(RetargetClip(source, ual.skeleton, hero), rig);
  }
  EXPECT_EQ(clips.size(), ual.clips.size());
}

}  // namespace

}  // namespace engine::animation

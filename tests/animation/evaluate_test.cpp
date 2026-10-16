#include "engine/animation/evaluate.h"

#include <bx/math.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "engine/animation/rig.h"
#include "engine/resource/animation_clip.h"
#include "engine/resource/cpu_mesh.h"
#include "engine/resource/gltf_reader.h"
#include "engine/resource/retarget.h"
#include "engine/resource/skeleton.h"

namespace engine::animation {

namespace {

/** @brief Path of a file under the source tree's assets/. */
auto Asset(const char* relative) -> std::filesystem::path {
  return std::filesystem::path(ENGINE_ASSET_DIR) / relative;
}

/** @brief Index of the joint named @p name, or the joint count if absent. */
auto JointIndex(const Skeleton& skeleton, const std::string& name)
    -> std::size_t {
  return static_cast<std::size_t>(
      std::ranges::find(skeleton.joints, name, &Joint::name) -
      skeleton.joints.begin());
}

/** @brief Translation of a model-space matrix. */
auto Position(const Mat4& m) -> bx::Vec3 {
  return bx::load<bx::Vec3>(&m.at(12));
}

/** @brief Largest difference between two poses' joint positions, in m. */
auto PoseDistance(const std::vector<Mat4>& a, const std::vector<Mat4>& b)
    -> float {
  float largest = 0.0F;
  for (std::size_t j = 0; j < a.size(); ++j) {
    largest = std::max(
        largest, bx::length(bx::sub(Position(a.at(j)), Position(b.at(j)))));
  }
  return largest;
}

// Skinning the bind pose with the rest palette must give back the bind pose;
// any joint order or matrix layout slip breaks it
TEST(Evaluate, RestPaletteSkinsTheBindPose) {
  for (const char* path : {"meshes/characters/Male_Casual.glb",
                           "meshes/characters/Superhero_Male.glb"}) {
    SCOPED_TRACE(path);
    const CpuMesh mesh = ReadGltf(Asset(path));
    const Skeleton skeleton = mesh.skeleton.value_or(Skeleton{});
    ASSERT_FALSE(skeleton.joints.empty());
    const Rig rig(skeleton);
    std::vector<Mat4> model;
    std::vector<Mat4> palette;
    RestPose(rig, model);
    Palette(skeleton, model, palette);

    for (std::size_t v = 0; v < mesh.positions.size(); ++v) {
      bx::Vec3 skinned{0.0F};
      for (std::size_t k = 0; k < 4; ++k) {
        const Mat4& m = palette.at(mesh.joints.at(v).at(k));
        skinned =
            bx::add(skinned, bx::mul(bx::mul(mesh.positions.at(v), m.data()),
                                     mesh.weights.at(v).at(k)));
      }
      ASSERT_NEAR(bx::length(bx::sub(skinned, mesh.positions.at(v))), 0.0F,
                  1e-3F)
          << "vertex " << v;
    }
  }
}

/** @brief The Superhero's rig with UAL clips retargeted onto it. */
class UalPoses : public testing::Test {
 protected:
  UalPoses() {
    const ClipFile ual = ReadGltfClips(Asset("animations/UAL1.glb"));
    for (const char* name :
         {"Walk_Loop", "Jog_Fwd_Loop", "Pistol_Aim_Neutral"}) {
      const auto it =
          std::ranges::find(ual.clips, std::string(name), &AnimationClip::name);
      clips_.emplace_back(RetargetClip(*it, ual.skeleton, hero_), rig_);
    }
  }

  /** @brief Evaluates @p layers into a fresh pose. */
  auto Pose(std::span<const LayerSample> layers) -> std::vector<Mat4> {
    std::vector<Mat4> model;
    evaluator_.Evaluate(rig_, layers, model);
    return model;
  }

  /** @brief The rest pose. */
  auto Rest() -> std::vector<Mat4> {
    std::vector<Mat4> model;
    RestPose(rig_, model);
    return model;
  }

  [[nodiscard]] auto Hero() const -> const Skeleton& { return hero_; }
  [[nodiscard]] auto Walk() const -> const Clip* { return &clips_.at(0); }
  [[nodiscard]] auto Jog() const -> const Clip* { return &clips_.at(1); }
  [[nodiscard]] auto Aim() const -> const Clip* { return &clips_.at(2); }

 private:
  Skeleton hero_ =
      ReadGltfClips(Asset("meshes/characters/Superhero_Male.glb")).skeleton;
  Rig rig_{hero_};
  std::vector<Clip> clips_;
  Evaluator evaluator_;
};

TEST_F(UalPoses, LoopingClipEndsWhereItStarts) {
  const std::array start{LayerSample{.clip = Walk(), .phase = 0.0F}};
  const std::array end{LayerSample{.clip = Walk(), .phase = 1.0F}};
  const std::array middle{LayerSample{.clip = Walk(), .phase = 0.5F}};
  EXPECT_LT(PoseDistance(Pose(start), Pose(end)), 1e-3F);
  EXPECT_GT(PoseDistance(Pose(start), Pose(middle)), 0.05F);
  EXPECT_GT(PoseDistance(Pose(start), Rest()), 0.05F);
}

TEST_F(UalPoses, HalfBlendPutsTheHipsHalfway) {
  const std::array walk_only{LayerSample{.clip = Walk(), .phase = 0.3F}};
  const std::array jog_only{LayerSample{.clip = Jog(), .phase = 0.3F}};
  const std::array half{LayerSample{
      .clip = Walk(), .blend_with = Jog(), .blend = 0.5F, .phase = 0.3F}};

  // The hips hang straight off the root, so their position blends linearly
  const std::size_t pelvis = JointIndex(Hero(), "pelvis");
  const bx::Vec3 a = Position(Pose(walk_only).at(pelvis));
  const bx::Vec3 b = Position(Pose(jog_only).at(pelvis));
  const bx::Vec3 mid = Position(Pose(half).at(pelvis));
  EXPECT_NEAR(bx::length(bx::sub(mid, bx::lerp(a, b, 0.5F))), 0.0F, 1e-3F);
  EXPECT_GT(bx::length(bx::sub(a, b)), 0.01F);
}

TEST_F(UalPoses, MaskedLayerLeavesTheLegsAlone) {
  // Upper body: spine_01 and everything under it
  const Skeleton& hero = Hero();
  std::vector<float> upper(hero.joints.size(), 0.0F);
  for (std::size_t j = 0; j < hero.joints.size(); ++j) {
    for (auto at = std::optional<std::uint32_t>(j); at;
         at = hero.joints.at(*at).parent) {
      if (hero.joints.at(*at).name == "spine_01") {
        upper.at(j) = 1.0F;
      }
    }
  }
  const std::array walk_only{LayerSample{.clip = Walk(), .phase = 0.4F}};
  const std::array aiming{
      LayerSample{.clip = Walk(), .phase = 0.4F},
      LayerSample{.clip = Aim(), .phase = 0.0F, .joint_weights = &upper}};
  const std::array masked_off{LayerSample{.clip = Walk(), .phase = 0.4F},
                              LayerSample{.clip = Aim(),
                                          .phase = 0.0F,
                                          .weight = 0.0F,
                                          .joint_weights = &upper}};

  const auto walking = Pose(walk_only);
  const auto layered = Pose(aiming);
  const std::size_t thigh = JointIndex(hero, "thigh_l");
  const std::size_t hand = JointIndex(hero, "hand_r");
  EXPECT_LT(bx::length(bx::sub(Position(layered.at(thigh)),
                               Position(walking.at(thigh)))),
            1e-4F);
  EXPECT_GT(bx::length(bx::sub(Position(layered.at(hand)),
                               Position(walking.at(hand)))),
            0.05F);
  EXPECT_LT(PoseDistance(Pose(masked_off), walking), 1e-4F);
}

}  // namespace

}  // namespace engine::animation

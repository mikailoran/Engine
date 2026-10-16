#include "engine/resource/ragdoll_fit.h"

#include <bx/math.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/animation/evaluate.h"
#include "engine/animation/rig.h"
#include "engine/physics/pose.h"
#include "engine/physics/ragdoll.h"
#include "engine/resource/gltf_reader.h"
#include "engine/resource/skeleton.h"
#include "engine/util/mat4.h"

namespace engine {

namespace {

/** @brief Path of a file under the source tree's assets/. */
auto Asset(const char* relative) -> std::filesystem::path {
  return std::filesystem::path(ENGINE_ASSET_DIR) / relative;
}

/** @brief The Superhero's skeleton and its rest pose. */
class HeroRagdoll : public testing::Test {
 protected:
  HeroRagdoll() {
    const animation::Rig rig(hero_);
    animation::RestPose(rig, rest_);
  }

  /** @brief The UAL ragdoll file fitted to the Superhero. */
  [[nodiscard]] auto Fit() const -> physics::RagdollDesc {
    return FitRagdoll(ReadRagdollFile(Asset("ragdolls/ual_humanoid.json")),
                      hero_, rest_);
  }

  /** @brief Rest position of the joint named @p name. */
  [[nodiscard]] auto RestPosition(const std::string& name) const -> bx::Vec3 {
    const auto it = std::ranges::find(hero_.joints, name, &Joint::name);
    const auto index = static_cast<std::size_t>(it - hero_.joints.begin());
    return bx::load<bx::Vec3>(&rest_.at(index).at(12));
  }

  [[nodiscard]] auto Hero() const -> const Skeleton& { return hero_; }
  [[nodiscard]] auto Rest() const -> const std::vector<Mat4>& { return rest_; }

 private:
  Skeleton hero_ =
      ReadGltfClips(Asset("meshes/characters/Superhero_Male.glb")).skeleton;
  std::vector<Mat4> rest_;
};

TEST_F(HeroRagdoll, FileFitsIntoATree) {
  const physics::RagdollDesc desc = Fit();
  ASSERT_EQ(desc.parts.size(), 11U);
  EXPECT_FALSE(desc.parts.front().parent.has_value());
  for (std::size_t i = 1; i < desc.parts.size(); ++i) {
    ASSERT_TRUE(desc.parts.at(i).parent.has_value()) << "part " << i;
    EXPECT_LT(desc.parts.at(i).parent.value_or(i), i);
  }
}

// The thigh capsule must run from the hip to the knee in rest pose
TEST_F(HeroRagdoll, CapsuleSpansItsBone) {
  const physics::RagdollDesc desc = Fit();
  const auto poses = PosesOf(Rest());
  const auto thigh = std::ranges::find_if(
      desc.parts, [&](const physics::RagdollPart& part) -> bool {
        return Hero().joints.at(part.joint).name == "thigh_l";
      });
  ASSERT_NE(thigh, desc.parts.end());

  const physics::Pose& joint = poses.at(thigh->joint);
  const bx::Vec3 centre =
      bx::add(joint.position, bx::mul(thigh->offset.position, joint.rotation));
  const bx::Vec3 hip = RestPosition("thigh_l");
  const bx::Vec3 knee = RestPosition("calf_l");
  EXPECT_LT(bx::length(bx::sub(centre, bx::lerp(hip, knee, 0.5F))), 1e-4F);

  const bx::Vec3 axis =
      bx::mul(bx::Vec3{0.0F, 1.0F, 0.0F},
              bx::mul(joint.rotation, thigh->offset.rotation));
  const bx::Vec3 bone = bx::normalize(bx::sub(knee, hip));
  EXPECT_NEAR(bx::dot(axis, bone), 1.0F, 1e-4F);
  EXPECT_NEAR((2.0F * thigh->half_height) + (2.0F * thigh->radius),
              bx::length(bx::sub(knee, hip)), 1e-4F);
}

TEST_F(HeroRagdoll, UnknownJointThrows) {
  RagdollPartSpec spec;
  spec.joint = "no_such_joint";
  EXPECT_THROW(FitRagdoll({spec}, Hero(), Rest()), std::runtime_error);
}

TEST_F(HeroRagdoll, ParentMustComeFirst) {
  RagdollPartSpec spec;
  spec.joint = "thigh_l";
  spec.end = "calf_l";
  spec.parent = "pelvis";
  EXPECT_THROW(FitRagdoll({spec}, Hero(), Rest()), std::runtime_error);
}

}  // namespace

}  // namespace engine

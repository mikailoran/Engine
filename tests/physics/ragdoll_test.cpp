// Ragdolls through the physics facade alone: no ECS, no bgfx.

#include "engine/physics/ragdoll.h"

#include <bx/math.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "engine/physics/jolt_runtime.h"
#include "engine/physics/physics_world.h"
#include "engine/physics/pose.h"

namespace engine {

namespace {

using physics::Pose;
using physics::RagdollHandle;
using physics::RagdollPart;

constexpr float kDt = 1.0F / 60.0F;
constexpr physics::UserTag kTag = 42;
constexpr bx::Vec3 kOrigin{0.0F, 0.0F, 0.0F};

/** @brief Hips, chest and head joints standing at @p base, unrotated. */
auto StandingJoints(const bx::Vec3& base) -> std::vector<Pose> {
  return {{.position = bx::add(base, {0.0F, 1.0F, 0.0F})},
          {.position = bx::add(base, {0.0F, 1.4F, 0.0F})},
          {.position = bx::add(base, {0.0F, 1.8F, 0.0F})}};
}

/** @brief A three-part chain: a capsule above each joint. */
auto ChainDesc() -> physics::RagdollDesc {
  const auto part = [](std::uint32_t joint) -> RagdollPart {
    RagdollPart out;
    out.joint = joint;
    if (joint > 0) {
      out.parent = joint - 1;
    }
    out.offset.position = {0.0F, 0.2F, 0.0F};
    out.half_height = 0.08F;
    out.radius = 0.1F;
    out.mass = 10.0F;
    return out;
  };
  return {.parts = {part(0), part(1), part(2)}};
}

/** @brief A world with a floor at y = 0 and a chain ragdoll's settings. */
class RagdollScene : public testing::Test {
 protected:
  RagdollScene() {
    physics::BodyDesc floor{};
    floor.shape.half_extents = {20.0F, 0.5F, 20.0F};
    floor.pose.position = {0.0F, -0.5F, 0.0F};
    world_.CreateBody(floor);
    settings_ =
        world_.CreateRagdollSettings(ChainDesc(), StandingJoints(kOrigin));
  }

  /** @brief Adds a chain ragdoll standing at @p base. */
  auto AddRagdoll(const bx::Vec3& base) -> RagdollHandle {
    return world_.CreateRagdoll(settings_, StandingJoints(base), kTag);
  }

  /** @brief Steps @p steps times at 60 Hz. */
  void Run(int steps) {
    for (int i = 0; i < steps; ++i) {
      world_.Step(kDt);
    }
  }

  /** @brief Where @p ragdoll's parts put its three joints. */
  auto Joints(RagdollHandle ragdoll) -> std::vector<Pose> {
    std::vector<Pose> joints(3);
    world_.RagdollJointPoses(ragdoll, joints);
    return joints;
  }

  auto World() -> physics::PhysicsWorld& { return world_; }

 private:
  physics::JoltRuntime runtime_;
  physics::PhysicsWorld world_{runtime_, 64};
  physics::RagdollSettingsHandle settings_;
};

TEST_F(RagdollScene, SettingsBuild) {
  EXPECT_TRUE(AddRagdoll(kOrigin).IsValid());
}

TEST_F(RagdollScene, KinematicRagdollFollowsItsPoses) {
  const RagdollHandle ragdoll = AddRagdoll(kOrigin);
  const auto moved = StandingJoints({2.0F, 0.0F, -1.0F});
  World().PoseRagdoll(ragdoll, moved, kDt);
  const auto joints = Joints(ragdoll);
  for (std::size_t i = 0; i < joints.size(); ++i) {
    EXPECT_NEAR(
        bx::length(bx::sub(joints.at(i).position, moved.at(i).position)), 0.0F,
        1e-4F)
        << "joint " << i;
  }
  // Kinematic: stepping leaves it where it was put
  Run(30);
  EXPECT_NEAR(Joints(ragdoll).at(0).position.y, 1.0F, 1e-4F);
}

TEST_F(RagdollScene, RayHitsAPartWithItsRagdollAndTag) {
  const RagdollHandle ragdoll = AddRagdoll(kOrigin);
  // The chest capsule's centre is 0.2 m above the chest joint
  const auto hits = World().CastRay(
      {.pos = {-5.0F, 1.6F, 0.0F}, .dir = {1.0F, 0.0F, 0.0F}}, 10.0F);
  ASSERT_FALSE(hits.empty());
  EXPECT_EQ(hits.front().ragdoll, ragdoll);
  EXPECT_EQ(hits.front().part, 1U);
  EXPECT_EQ(hits.front().user_data, kTag);
  EXPECT_FALSE(hits.front().body.IsValid());
  EXPECT_NEAR(hits.front().distance, 4.9F, 1e-3F);
}

TEST_F(RagdollScene, PartsAreNotListedAsBodies) {
  const RagdollHandle ragdoll = AddRagdoll(kOrigin);
  EXPECT_EQ(World().Bodies().size(), 1U);  // the floor
  const auto ragdolls = World().Ragdolls();
  ASSERT_EQ(ragdolls.size(), 1U);
  EXPECT_EQ(ragdolls.front().ragdoll, ragdoll);
  EXPECT_EQ(ragdolls.front().user_data, kTag);
  EXPECT_EQ(World().UserData(ragdoll), kTag);
}

TEST_F(RagdollScene, SimulatedRagdollFallsAndRestsOnTheFloor) {
  const RagdollHandle ragdoll = AddRagdoll(kOrigin);
  World().SetRagdollSimulated(ragdoll, true);
  Run(240);
  const auto joints = Joints(ragdoll);
  // It collapsed: the head is far below where it stood
  EXPECT_LT(joints.at(2).position.y, 1.0F);
  for (const Pose& joint : joints) {
    EXPECT_GT(joint.position.y, -0.15F);
  }
  // Posing a simulated ragdoll is ignored
  World().PoseRagdoll(ragdoll, StandingJoints(kOrigin), kDt);
  EXPECT_LT(Joints(ragdoll).at(2).position.y, 1.0F);
}

TEST_F(RagdollScene, LetGoRagdollKeepsItsMotion) {
  const RagdollHandle ragdoll = AddRagdoll(kOrigin);
  // Posed 5 cm along +X per step: moving at 3 m/s
  World().PoseRagdoll(ragdoll, StandingJoints({0.05F, 0.0F, 0.0F}), kDt);
  World().SetRagdollSimulated(ragdoll, true);
  World().Step(kDt);
  EXPECT_GT(Joints(ragdoll).at(0).position.x, 0.08F);
}

TEST_F(RagdollScene, CharactersWalkThroughRagdolls) {
  AddRagdoll(kOrigin);
  const auto character = World().CreateCharacter({.feet = {-2.0F, 0.0F, 0.0F}});
  for (int i = 0; i < 120; ++i) {
    World().SetCharacterVelocity(character, {2.0F, 0.0F, 0.0F});
    World().Step(kDt);
  }
  EXPECT_GT(World().GetCharacter(character).feet.x, 1.5F);
}

TEST_F(RagdollScene, DestroyedRagdollIsGone) {
  const RagdollHandle ragdoll = AddRagdoll(kOrigin);
  World().DestroyRagdoll(ragdoll);
  EXPECT_TRUE(World().Ragdolls().empty());
  EXPECT_FALSE(World().UserData(ragdoll).has_value());
  const auto hits = World().CastRay(
      {.pos = {-5.0F, 1.6F, 0.0F}, .dir = {1.0F, 0.0F, 0.0F}}, 10.0F);
  EXPECT_TRUE(hits.empty());
  // A stale handle is safe
  World().DestroyRagdoll(ragdoll);
}

TEST_F(RagdollScene, NonTargetCharacterIsNotHitByRays) {
  World().CreateCharacter({.feet = {0.0F, 0.0F, 3.0F}, .ray_target = false});
  const auto hits = World().CastRay(
      {.pos = {-5.0F, 1.0F, 3.0F}, .dir = {1.0F, 0.0F, 0.0F}}, 10.0F);
  EXPECT_TRUE(hits.empty());
}

}  // namespace

}  // namespace engine

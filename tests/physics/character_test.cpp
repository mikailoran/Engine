// Characters through the physics facade alone: no ECS, no bgfx.

#include <bx/math.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "engine/physics/body_handle.h"
#include "engine/physics/character_handle.h"
#include "engine/physics/collision_mesh_handle.h"
#include "engine/physics/jolt_runtime.h"
#include "engine/physics/physics_world.h"
#include "engine/physics/shape.h"

namespace engine {

namespace {

using physics::BodyHandle;
using physics::CharacterHandle;

constexpr float kStep = 1.0F / 60.0F;

/** @brief A world with a few static or dynamic boxes and characters. */
class CharacterScene {
 public:
  /** @brief Adds a box of @p half_extents centred at @p pose's position. */
  auto AddBox(const physics::Pose& pose, const bx::Vec3& half_extents,
              physics::Motion motion = physics::Motion::kStatic) -> BodyHandle {
    physics::BodyDesc desc{};
    desc.shape.half_extents = half_extents;
    desc.pose = pose;
    desc.motion = motion;
    desc.material.restitution = 0.0F;
    return world_.CreateBody(desc);
  }

  /** @brief A 40 m square floor whose top is at y = 0. */
  auto AddFloor() -> BodyHandle {
    return AddBox({.position = {0.0F, -0.5F, 0.0F}}, {20.0F, 0.5F, 20.0F});
  }

  /** @brief Adds a default character standing at @p feet. */
  auto AddCharacter(const bx::Vec3& feet) -> CharacterHandle {
    return world_.CreateCharacter({.feet = feet});
  }

  /** @brief Steps @p seconds at 60 Hz, rounded to whole steps. */
  void Run(float seconds) {
    const std::int64_t steps = std::lround(seconds / kStep);
    for (std::int64_t i = 0; i < steps; ++i) {
      world_.Step(kStep);
    }
  }

  /** @brief The world, for direct calls. */
  auto World() -> physics::PhysicsWorld& { return world_; }

 private:
  physics::JoltRuntime runtime_;
  physics::PhysicsWorld world_{runtime_, 1024};
};

/**
 * @brief Highest the feet get walking +X at 2 m/s for 4 s into a ramp tilted
 * by @p slope_deg, rising along +X from the origin.
 */
auto HighestOnRamp(float slope_deg) -> float {
  CharacterScene scene;
  scene.AddFloor();
  // An 8 x 0.5 m slab whose top surface's low edge sits at the origin
  const float angle = bx::toRad(slope_deg);
  const bx::Vec3 centre{(4.0F * std::cos(angle)) + (0.25F * std::sin(angle)),
                        (4.0F * std::sin(angle)) - (0.25F * std::cos(angle)),
                        0.0F};
  scene.AddBox({.position = centre,
                .rotation = bx::fromAxisAngle({0.0F, 0.0F, 1.0F}, angle)},
               {4.0F, 0.25F, 2.0F});
  const CharacterHandle walker = scene.AddCharacter({-1.0F, 0.0F, 0.0F});
  scene.Run(0.5F);
  scene.World().SetCharacterVelocity(walker, {2.0F, 0.0F, 0.0F});

  float highest = 0.0F;
  for (int i = 0; i < 240; ++i) {
    scene.Run(kStep);
    highest = std::max(highest, scene.World().GetCharacter(walker).feet.y);
  }
  return highest;
}

TEST(Character, StandsOnTheFloor) {
  CharacterScene scene;
  scene.AddFloor();
  const CharacterHandle character = scene.AddCharacter({0.0F, 1.0F, 0.0F});
  scene.Run(2.0F);
  const physics::CharacterState state = scene.World().GetCharacter(character);
  EXPECT_TRUE(state.on_ground);
  EXPECT_NEAR(state.feet.y, 0.0F, 0.05F);
}

TEST(Character, FallsUnderGravityWithNothingBelow) {
  CharacterScene scene;
  const CharacterHandle character = scene.AddCharacter({0.0F, 0.0F, 0.0F});
  scene.Run(1.0F);
  const physics::CharacterState state = scene.World().GetCharacter(character);
  EXPECT_FALSE(state.on_ground);
  // Speed updates before position each step: g dt^2 n(n + 1) / 2 for n = 60
  EXPECT_NEAR(state.feet.y, -4.99F, 0.02F);
  EXPECT_NEAR(state.velocity.y, -9.81F, 0.02F);
}

TEST(Character, StopsAtAWall) {
  CharacterScene scene;
  scene.AddFloor();
  // Its near face is at x = 2.5
  scene.AddBox({.position = {3.0F, 1.0F, 0.0F}}, {0.5F, 1.0F, 5.0F});
  const CharacterHandle character = scene.AddCharacter({0.0F, 0.0F, 0.0F});
  scene.World().SetCharacterVelocity(character, {3.0F, 0.0F, 0.0F});
  scene.Run(3.0F);
  // The capsule's 0.3 m radius keeps its centre short of the face
  EXPECT_NEAR(scene.World().GetCharacter(character).feet.x, 2.2F, 0.05F);
}

TEST(Character, ClimbsAGentleSlopeButNotASteepOne) {
  // The default walkable limit is 45 degrees
  EXPECT_GT(HighestOnRamp(20.0F), 1.5F);
  EXPECT_LT(HighestOnRamp(60.0F), 0.5F);
}

TEST(Character, JumpsAndLands) {
  CharacterScene scene;
  scene.AddFloor();
  const CharacterHandle character = scene.AddCharacter({0.0F, 0.0F, 0.0F});
  scene.Run(0.5F);
  scene.World().SetCharacterVelocity(character, {0.0F, 5.0F, 0.0F});
  scene.Run(0.25F);
  // 5 t - g t^2 / 2 is about 0.94 m after a quarter second
  EXPECT_GT(scene.World().GetCharacter(character).feet.y, 0.8F);
  scene.Run(2.0F);
  const physics::CharacterState state = scene.World().GetCharacter(character);
  EXPECT_TRUE(state.on_ground);
  EXPECT_NEAR(state.feet.y, 0.0F, 0.05F);
}

TEST(Character, PushesADynamicBody) {
  CharacterScene scene;
  scene.AddFloor();
  // A rolling 65 kg ball: well within the default 200 N push
  physics::BodyDesc ball{};
  ball.shape.kind = physics::ShapeKind::kSphere;
  ball.shape.radius = 0.25F;
  ball.pose.position = {1.5F, 0.25F, 0.0F};
  ball.motion = physics::Motion::kDynamic;
  const BodyHandle pushed = scene.World().CreateBody(ball);
  const CharacterHandle character = scene.AddCharacter({0.0F, 0.0F, 0.0F});
  scene.World().SetCharacterVelocity(character, {2.0F, 0.0F, 0.0F});
  scene.Run(2.0F);
  EXPECT_GT(scene.World().GetPose(pushed).position.x, 2.5F);
}

TEST(Character, StandsOnAMeshFloor) {
  CharacterScene scene;
  // Seen from above (+X right, -Z up on screen), both triangles run CCW
  const physics::CollisionMeshHandle mesh =
      scene.World().CreateCollisionMesh({.vertices = {{-10.0F, 0.0F, -10.0F},
                                                      {10.0F, 0.0F, -10.0F},
                                                      {10.0F, 0.0F, 10.0F},
                                                      {-10.0F, 0.0F, 10.0F}},
                                         .indices = {0, 2, 1, 0, 3, 2}});
  physics::BodyDesc floor{};
  floor.shape.kind = physics::ShapeKind::kMesh;
  floor.shape.mesh = mesh;
  scene.World().CreateBody(floor);
  const CharacterHandle character = scene.AddCharacter({0.0F, 1.0F, 0.0F});
  scene.Run(2.0F);
  const physics::CharacterState state = scene.World().GetCharacter(character);
  EXPECT_TRUE(state.on_ground);
  EXPECT_NEAR(state.feet.y, 0.0F, 0.05F);
}

TEST(Character, TeleportMovesTheFeet) {
  CharacterScene scene;
  const CharacterHandle character = scene.AddCharacter({0.0F, 0.0F, 0.0F});
  scene.World().SetCharacterFeet(character, {4.0F, 5.0F, -6.0F});
  const bx::Vec3 feet = scene.World().GetCharacter(character).feet;
  EXPECT_FLOAT_EQ(feet.x, 4.0F);
  EXPECT_FLOAT_EQ(feet.y, 5.0F);
  EXPECT_FLOAT_EQ(feet.z, -6.0F);
}

TEST(Character, DestroyedCharacterLeavesTheList) {
  CharacterScene scene;
  const CharacterHandle kept = scene.World().CreateCharacter({.user_data = 7U});
  const CharacterHandle doomed = scene.AddCharacter({2.0F, 0.0F, 0.0F});
  scene.World().DestroyCharacter(doomed);
  // A stale handle is safe to destroy again
  scene.World().DestroyCharacter(doomed);

  const auto characters = scene.World().Characters();
  ASSERT_EQ(characters.size(), 1U);
  EXPECT_EQ(characters.front().character, kept);
  EXPECT_EQ(characters.front().user_data, std::uint64_t{7});
}

}  // namespace

}  // namespace engine

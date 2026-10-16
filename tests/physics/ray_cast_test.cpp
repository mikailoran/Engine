// Ray casts through the physics facade alone: no ECS, no bgfx.

#include <bx/bounds.h>
#include <bx/bx.h>
#include <bx/constants.h>
#include <bx/math.h>
#include <gtest/gtest.h>

#include <vector>

#include "engine/physics/body_handle.h"
#include "engine/physics/character_handle.h"
#include "engine/physics/collision_mesh_handle.h"
#include "engine/physics/hitbox_handle.h"
#include "engine/physics/jolt_runtime.h"
#include "engine/physics/physics_world.h"
#include "engine/physics/shape.h"

namespace engine {

namespace {

using physics::BodyHandle;
using physics::CharacterHandle;
using physics::ConvexPart;
using physics::HitboxHandle;
using physics::RayHit;

constexpr float kTolerance = 1e-3F;

/** @brief A world to cast rays in, with nothing stepped. */
class RayScene {
 public:
  /** @brief Adds a static unit box (half extent 0.5) centred at @p centre. */
  auto AddBox(const bx::Vec3& centre, physics::UserTag tag = 0) -> BodyHandle {
    physics::BodyDesc desc{};
    desc.pose.position = centre;
    desc.user_data = tag;
    return world_.CreateBody(desc);
  }

  /** @brief Adds a static 2 m square mesh at y = 0, front face up. */
  auto AddFloorMesh() -> BodyHandle {
    const physics::CollisionMeshHandle mesh =
        world_.CreateCollisionMesh({.vertices = {{-1.0F, 0.0F, -1.0F},
                                                 {1.0F, 0.0F, -1.0F},
                                                 {1.0F, 0.0F, 1.0F},
                                                 {-1.0F, 0.0F, 1.0F}},
                                    .indices = {0, 2, 1, 0, 3, 2}});
    physics::BodyDesc desc{};
    desc.shape.kind = physics::ShapeKind::kMesh;
    desc.shape.mesh = mesh;
    return world_.CreateBody(desc);
  }

  /** @brief Adds a default character (1.8 m tall, 0.3 m radius). */
  auto AddCharacter(const bx::Vec3& feet, physics::UserTag tag)
      -> CharacterHandle {
    return world_.CreateCharacter({.feet = feet, .user_data = tag});
  }

  /** @brief Adds a default character whose rays hit @p hitbox instead. */
  auto AddCharacter(const bx::Vec3& feet, HitboxHandle hitbox)
      -> CharacterHandle {
    return world_.CreateCharacter({.feet = feet, .hitbox = hitbox});
  }

  /** @brief The world, for calls the helpers don't cover. */
  auto World() -> physics::PhysicsWorld& { return world_; }

  /** @brief Casts from @p origin along unit @p dir up to @p max_distance. */
  [[nodiscard]] auto Cast(const bx::Vec3& origin, const bx::Vec3& dir,
                          float max_distance = 100.0F) const
      -> std::vector<RayHit> {
    return world_.CastRay({.pos = origin, .dir = dir}, max_distance);
  }

 private:
  physics::JoltRuntime runtime_;
  physics::PhysicsWorld world_{runtime_, 64};
};

constexpr bx::Vec3 kPlusX{1.0F, 0.0F, 0.0F};
constexpr bx::Vec3 kOrigin{0.0F, 0.0F, 0.0F};
constexpr bx::Vec3 kMinusZ{0.0F, 0.0F, -1.0F};

/** @brief The 8 corners of @p box. */
auto BoxPart(const bx::Aabb& box) -> ConvexPart {
  ConvexPart part;
  for (const float x : {box.min.x, box.max.x}) {
    for (const float y : {box.min.y, box.max.y}) {
      for (const float z : {box.min.z, box.max.z}) {
        part.points.emplace_back(x, y, z);
      }
    }
  }
  return part;
}

/** @brief A torso 0.4 m wide, 0.3 m deep, from 0.5 m up to 1.5 m. */
constexpr bx::Aabb kTorso{.min = {-0.2F, 0.5F, -0.15F},
                          .max = {0.2F, 1.5F, 0.15F}};

/**
 * @brief A T-pose stand-in, in hitbox space (feet at the origin): a torso
 * 0.4 m wide up to 1.5 m, and one arm reaching from x 0.4 to 1.0 at 1.4 m.
 */
auto TPoseParts() -> std::vector<ConvexPart> {
  return {BoxPart(kTorso),
          BoxPart({.min = {0.4F, 1.35F, -0.05F}, .max = {1.0F, 1.45F, 0.05F}})};
}

TEST(RayCast, ReportsEveryBodyNearestFirst) {
  RayScene scene;
  const BodyHandle far = scene.AddBox({8.0F, 0.0F, 0.0F}, 2);
  const BodyHandle near = scene.AddBox({4.0F, 0.0F, 0.0F}, 1);

  const auto hits = scene.Cast({0.0F, 0.0F, 0.0F}, kPlusX);
  ASSERT_EQ(hits.size(), 2U);
  EXPECT_EQ(hits.at(0).body, near);
  EXPECT_EQ(hits.at(0).user_data, 1U);
  EXPECT_FALSE(hits.at(0).character.IsValid());
  EXPECT_NEAR(hits.at(0).distance, 3.5F, kTolerance);
  EXPECT_NEAR(hits.at(0).point.x, 3.5F, kTolerance);
  EXPECT_EQ(hits.at(1).body, far);
  EXPECT_NEAR(hits.at(1).distance, 7.5F, kTolerance);
}

TEST(RayCast, StopsAtMaxDistance) {
  RayScene scene;
  scene.AddBox({4.0F, 0.0F, 0.0F});
  EXPECT_TRUE(scene.Cast({0.0F, 0.0F, 0.0F}, kPlusX, 3.0F).empty());
}

TEST(RayCast, MissesWhatIsBehindIt) {
  RayScene scene;
  scene.AddBox({4.0F, 0.0F, 0.0F});
  EXPECT_TRUE(scene.Cast({0.0F, 0.0F, 0.0F}, {-1.0F, 0.0F, 0.0F}).empty());
}

TEST(RayCast, HitsACharacterCapsule) {
  RayScene scene;
  const CharacterHandle character = scene.AddCharacter({5.0F, 0.0F, 0.0F}, 7);

  const auto hits = scene.Cast({0.0F, 1.0F, 0.0F}, kPlusX);
  ASSERT_EQ(hits.size(), 1U);
  EXPECT_EQ(hits.at(0).character, character);
  EXPECT_FALSE(hits.at(0).body.IsValid());
  EXPECT_EQ(hits.at(0).user_data, 7U);
  EXPECT_NEAR(hits.at(0).distance, 4.7F, kTolerance);
}

TEST(RayCast, HitsAMeshFromTheFrontOnly) {
  RayScene scene;
  const BodyHandle floor = scene.AddFloorMesh();

  const auto from_above = scene.Cast({0.0F, 2.0F, 0.0F}, {0.0F, -1.0F, 0.0F});
  ASSERT_EQ(from_above.size(), 1U);
  EXPECT_EQ(from_above.at(0).body, floor);
  EXPECT_NEAR(from_above.at(0).distance, 2.0F, kTolerance);

  EXPECT_TRUE(scene.Cast({0.0F, -2.0F, 0.0F}, {0.0F, 1.0F, 0.0F}).empty());
}

TEST(RayCast, StartingInsideAShapeHitsItAtZero) {
  RayScene scene;
  const CharacterHandle character = scene.AddCharacter({0.0F, 0.0F, 0.0F}, 1);
  const BodyHandle wall = scene.AddBox({4.0F, 1.0F, 0.0F});

  const auto hits = scene.Cast({0.0F, 1.0F, 0.0F}, kPlusX);
  ASSERT_EQ(hits.size(), 2U);
  EXPECT_EQ(hits.at(0).character, character);
  EXPECT_NEAR(hits.at(0).distance, 0.0F, kTolerance);
  EXPECT_EQ(hits.at(1).body, wall);
}

TEST(RayCast, HitsACharacterHitboxPart) {
  RayScene scene;
  const HitboxHandle hitbox = scene.World().CreateHitbox(TPoseParts());
  ASSERT_TRUE(hitbox.IsValid());
  const CharacterHandle character = scene.AddCharacter(kOrigin, hitbox);

  // Down the arm's middle, from 5 m in front
  const auto hits = scene.Cast({0.7F, 1.4F, 5.0F}, kMinusZ);
  ASSERT_EQ(hits.size(), 1U);
  EXPECT_EQ(hits.at(0).character, character);
  EXPECT_NEAR(hits.at(0).distance, 4.95F, kTolerance);
}

TEST(RayCast, AHitboxReplacesTheCapsule) {
  RayScene scene;
  scene.AddCharacter(kOrigin, scene.World().CreateHitbox(TPoseParts()));

  // Inside the 0.3 m capsule, outside the 0.2 m torso
  EXPECT_TRUE(scene.Cast({0.25F, 1.0F, 5.0F}, kMinusZ).empty());
  // Between the arm and the hip
  EXPECT_TRUE(scene.Cast({0.7F, 1.0F, 5.0F}, kMinusZ).empty());
}

TEST(RayCast, AHitboxTurnsWithItsPose) {
  RayScene scene;
  const CharacterHandle character =
      scene.AddCharacter(kOrigin, scene.World().CreateHitbox(TPoseParts()));

  // A quarter turn left swings the arm from +X to -Z
  const bx::Quaternion turn =
      bx::fromAxisAngle({0.0F, 1.0F, 0.0F}, bx::kPiHalf);
  scene.World().SetHitboxPose(character, turn, {1.0F, 1.0F, 1.0F});

  EXPECT_TRUE(scene.Cast({0.7F, 1.4F, 5.0F}, kMinusZ).empty());
  const auto hits = scene.Cast({0.0F, 1.4F, -5.0F}, {0.0F, 0.0F, 1.0F});
  ASSERT_FALSE(hits.empty());
  EXPECT_EQ(hits.at(0).character, character);
  EXPECT_NEAR(hits.at(0).distance, 4.0F, kTolerance);
}

TEST(RayCast, AHitboxScalesAndFollowsTheFeet) {
  RayScene scene;
  const CharacterHandle character = scene.AddCharacter(
      {10.0F, 0.0F, 0.0F}, scene.World().CreateHitbox(TPoseParts()));
  scene.World().SetHitboxPose(character, bx::Quaternion{bx::InitIdentity},
                              {2.0F, 2.0F, 2.0F});

  // Twice as big, the arm reaches x 12 at a height of 2.8 m
  const auto hits = scene.Cast({11.9F, 2.8F, 5.0F}, kMinusZ);
  ASSERT_EQ(hits.size(), 1U);
  EXPECT_EQ(hits.at(0).character, character);
  EXPECT_NEAR(hits.at(0).distance, 4.9F, kTolerance);
}

TEST(RayCast, CreateHitboxSkipsPartsWithoutVolume) {
  RayScene scene;
  const ConvexPart triangle{
      .points = {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}}};

  EXPECT_FALSE(scene.World().CreateHitbox({triangle}).IsValid());
  const HitboxHandle hitbox =
      scene.World().CreateHitbox({triangle, BoxPart(kTorso)});
  ASSERT_TRUE(hitbox.IsValid());
  scene.AddCharacter(kOrigin, hitbox);
  EXPECT_FALSE(scene.Cast({0.0F, 1.0F, 5.0F}, kMinusZ).empty());
}

}  // namespace

}  // namespace engine

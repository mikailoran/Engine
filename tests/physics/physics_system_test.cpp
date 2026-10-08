// Physics end to end: ECS components in, Jolt simulation, components out.
// Scene pieces copy debug.json's values; nothing here needs bgfx or meshes.

#include "engine/ecs/systems/physics_system.h"

#include <bx/bounds.h>
#include <bx/bx.h>
#include <bx/math.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "engine/ecs/components/character_body.h"
#include "engine/ecs/components/character_link.h"
#include "engine/ecs/components/collider.h"
#include "engine/ecs/components/physics_link.h"
#include "engine/ecs/components/rigid_body.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/math/rotation.h"
#include "engine/physics/body_handle.h"
#include "engine/physics/collision_mesh_handle.h"
#include "engine/physics/jolt_runtime.h"
#include "engine/physics/physics_world.h"
#include "engine/physics/shape.h"
#include "engine/platform/frame_context.h"

namespace engine {

namespace {

/** @brief A Transform at @p position with optional rotation and scale. */
auto At(const bx::Vec3& position,
        const bx::Quaternion& rotation = bx::Quaternion{bx::InitIdentity},
        const bx::Vec3& scale = {1.0F, 1.0F, 1.0F}) -> Transform {
  Transform transform{};
  transform.position = position;
  transform.rotation = rotation;
  transform.scale = scale;
  return transform;
}

/** @brief @p collider with its restitution replaced. */
auto WithRestitution(Collider collider, float restitution) -> Collider {
  collider.material.restitution = restitution;
  return collider;
}

/** @brief A sphere Collider of @p radius, before scale. */
auto Sphere(float radius) -> Collider {
  Collider collider{};
  collider.shape.kind = physics::ShapeKind::kSphere;
  collider.shape.radius = radius;
  return collider;
}

/**
 * @brief A square at y = 0, @p half wide on X and Z, front face up as the
 * renderer winds it.
 */
auto FloorSquare(float half) -> physics::TriangleMesh {
  // Seen from above (+X right, -Z up on screen), both triangles run CCW
  return {.vertices = {{-half, 0.0F, -half},
                       {half, 0.0F, -half},
                       {half, 0.0F, half},
                       {-half, 0.0F, half}},
          .indices = {0, 2, 1, 0, 3, 2}};
}

/** @brief A mesh Collider over @p mesh that doesn't bounce. */
auto MeshCollider(physics::CollisionMeshHandle mesh) -> Collider {
  Collider collider{};
  collider.shape.kind = physics::ShapeKind::kMesh;
  collider.shape.mesh = mesh;
  collider.material.restitution = 0.0F;
  return collider;
}

/** @brief Largest |dot(local axis, up)| as rendered: 1 means face-down. */
auto Upness(const Transform& transform) -> float {
  const std::array<float, 16> mtx =
      ModelMatrix(At({0.0F, 0.0F, 0.0F}, transform.rotation));
  float best = 0.0F;
  for (const bx::Vec3 axis :
       {bx::Vec3{1.0F, 0.0F, 0.0F}, bx::Vec3{0.0F, 1.0F, 0.0F},
        bx::Vec3{0.0F, 0.0F, 1.0F}}) {
    best = std::max(best, std::abs(bx::mul(axis, mtx.data()).y));
  }
  return best;
}

/**
 * @brief An ECS world wired to a PhysicsWorld, owned in Game's order: Jolt's
 * runtime, then the physics world, then the ECS and the system.
 */
class PhysicsHarness {
 public:
  /** @brief Registers the components physics reads and writes. */
  PhysicsHarness() {
    ecs_.RegisterComponent<Transform>();
    ecs_.RegisterComponent<Collider>();
    ecs_.RegisterComponent<RigidBody>();
    ecs_.RegisterComponent<PhysicsLink>();
    ecs_.RegisterComponent<CharacterBody>();
    ecs_.RegisterComponent<CharacterLink>();
  }

  /** @brief Runs @p frames frames of exactly one 60 Hz step each. */
  void Run(int frames) {
    FrameContext ctx{};
    ctx.dt = 1.0F / 60.0F;
    for (int i = 0; i < frames; ++i) {
      system_.Update(ecs_, world_, ctx);
      ecs_.Flush();
    }
  }

  /** @brief Adds an entity with a Transform and a Collider: a static body. */
  auto AddStatic(const Transform& transform, const Collider& collider = {})
      -> Entity {
    const Entity entity = ecs_.CreateEntity();
    ecs_.AddComponent(entity, transform);
    ecs_.AddComponent(entity, collider);
    return entity;
  }

  /** @brief Adds a static body plus a RigidBody: a dynamic body. */
  auto AddDynamic(const Transform& transform, const Collider& collider = {})
      -> Entity {
    const Entity entity = AddStatic(transform, collider);
    ecs_.AddComponent(entity, RigidBody{});
    return entity;
  }

  /** @brief Adds a default character whose feet are at @p feet. */
  auto AddCharacter(const bx::Vec3& feet) -> Entity {
    const Entity entity = ecs_.CreateEntity();
    ecs_.AddComponent(entity, At(feet));
    ecs_.AddComponent(entity, CharacterBody{});
    return entity;
  }

  /** @brief debug.json's floor; its top is at y = 0. */
  auto AddFloor() -> Entity {
    return AddStatic(At({0.0F, -0.1F, 0.0F}, bx::Quaternion{bx::InitIdentity},
                        {40.0F, 0.2F, 40.0F}));
  }

  /** @brief debug.json's east wall; its inner face is at x = 19.75. */
  auto AddWallEast() -> Entity {
    return AddStatic(At({20.0F, 2.0F, 0.0F}, bx::Quaternion{bx::InitIdentity},
                        {0.5F, 4.0F, 40.0F}));
  }

  /** @brief debug.json's ramp: 6 x 0.3 x 8, tilted 15 degrees about X. */
  auto AddRamp(float restitution = 0.6F) -> Entity {
    return AddStatic(
        At({0.0F, 1.0F, -14.0F}, EulerToQuat({bx::toRad(15.0F), 0.0F, 0.0F}),
           {6.0F, 0.3F, 8.0F}),
        WithRestitution(Collider{}, restitution));
  }

  /** @brief Builds a collision mesh in the physics world. */
  auto CreateCollisionMesh(const physics::TriangleMesh& mesh)
      -> physics::CollisionMeshHandle {
    return world_.CreateCollisionMesh(mesh);
  }

  /** @brief @p entity's @p Component. @pre It has one. */
  template <class Component>
  auto Get(Entity entity) -> Component& {
    return ecs_.GetComponent<Component>(entity);
  }

  /** @brief The ECS, for structural changes. */
  auto Entities() -> Ecs& { return ecs_; }

  /** @brief How many bodies the physics world holds. */
  [[nodiscard]] auto BodyCount() const -> std::size_t {
    return world_.Bodies().size();
  }

  /** @brief How many characters the physics world holds. */
  [[nodiscard]] auto CharacterCount() const -> std::size_t {
    return world_.Characters().size();
  }

  /** @brief How many entities are linked to a body. */
  auto LinkCount() -> std::size_t {
    std::size_t count = 0;
    ecs_.View<PhysicsLink>().ForEach(
        [&count](Entity, const PhysicsLink&) -> void { ++count; });
    return count;
  }

 private:
  physics::JoltRuntime runtime_;
  physics::PhysicsWorld world_{runtime_,
                               static_cast<std::uint32_t>(kMaxEntities)};
  Ecs ecs_;
  PhysicsSystem system_;
};

// --- Bodies follow the components ------------------------------------------

TEST(Physics, CreatesOneBodyPerCollider) {
  PhysicsHarness scene;
  scene.AddFloor();
  scene.AddWallEast();
  scene.AddRamp();
  scene.AddStatic(At({-3.0F, 0.5F, 6.0F}), Sphere(0.5F));
  scene.Run(1);
  EXPECT_EQ(scene.BodyCount(), 4U);
  EXPECT_EQ(scene.LinkCount(), 4U);
}

TEST(Physics, DestroyedEntityLosesItsBody) {
  PhysicsHarness scene;
  const Entity box = scene.AddDynamic(At({0.0F, 5.0F, 0.0F}));
  scene.Run(1);
  ASSERT_EQ(scene.BodyCount(), 1U);
  scene.Entities().DestroyEntity(box);
  scene.Entities().Flush();
  scene.Run(1);
  EXPECT_EQ(scene.BodyCount(), 0U);
}

TEST(Physics, RemovingColliderFreezesEntityAndReAddingRevivesIt) {
  PhysicsHarness scene;
  const Entity box = scene.AddDynamic(At({0.0F, 5.0F, 0.0F}));
  scene.Run(1);
  scene.Entities().RemoveComponent<Collider>(box);
  const float frozen_y = scene.Get<Transform>(box).position.y;
  scene.Run(30);
  EXPECT_EQ(scene.Get<Transform>(box).position.y, frozen_y);
  EXPECT_EQ(scene.BodyCount(), 0U);

  scene.Entities().AddComponent(box, Collider{});
  scene.Run(30);
  EXPECT_LT(scene.Get<Transform>(box).position.y, frozen_y - 0.5F);
}

TEST(Physics, RemovingRigidBodyKeepsTheBodyButMakesItStatic) {
  PhysicsHarness scene;
  const Entity box = scene.AddDynamic(At({0.0F, 5.0F, 0.0F}));
  scene.Run(1);
  const physics::BodyHandle body = scene.Get<PhysicsLink>(box).Body();

  scene.Entities().RemoveComponent<RigidBody>(box);
  scene.Run(1);
  const float static_y = scene.Get<Transform>(box).position.y;
  scene.Run(30);
  EXPECT_EQ(scene.Get<PhysicsLink>(box).Body(), body);
  EXPECT_EQ(scene.Get<Transform>(box).position.y, static_y);

  scene.Entities().AddComponent(box, RigidBody{});
  scene.Run(30);
  EXPECT_EQ(scene.Get<PhysicsLink>(box).Body(), body);
  EXPECT_LT(scene.Get<Transform>(box).position.y, static_y - 0.5F);
}

TEST(Physics, SameFrameColliderSwapKeepsBodyAndTakesNewShape) {
  PhysicsHarness scene;
  scene.AddFloor();
  const Entity box = scene.AddDynamic(At({0.0F, 0.5F, 0.0F}),
                                      WithRestitution(Collider{}, 0.0F));
  scene.Run(60);
  const physics::BodyHandle body = scene.Get<PhysicsLink>(box).Body();

  scene.Entities().RemoveComponent<Collider>(box);
  scene.Entities().AddComponent(box, WithRestitution(Sphere(1.0F), 0.0F));
  scene.Run(120);
  EXPECT_EQ(scene.Get<PhysicsLink>(box).Body(), body);
  // Now a radius 1 sphere, so it rests a unit above the floor
  EXPECT_NEAR(scene.Get<Transform>(box).position.y, 1.0F, 0.03F);
}

TEST(Physics, ReusedEntityIdGetsAFreshBody) {
  PhysicsHarness scene;
  const Entity doomed = scene.AddDynamic(At({3.0F, 4.0F, 3.0F}));
  scene.Run(1);
  const physics::BodyHandle old_body = scene.Get<PhysicsLink>(doomed).Body();
  scene.Entities().DestroyEntity(doomed);
  scene.Entities().Flush();

  // Ids are recycled FIFO: draw them until the doomed one comes back, before
  // PhysicsSystem has seen the destruction
  std::vector<Entity> fillers;
  Entity reused = scene.Entities().CreateEntity();
  while (reused != doomed) {
    fillers.push_back(reused);
    reused = scene.Entities().CreateEntity();
  }
  scene.Entities().AddComponent(reused, At({-5.0F, 6.0F, 5.0F}));
  scene.Entities().AddComponent(reused, Collider{});
  scene.Entities().AddComponent(reused, RigidBody{});
  for (const Entity filler : fillers) {
    scene.Entities().DestroyEntity(filler);
  }
  scene.Run(1);

  EXPECT_NE(scene.Get<PhysicsLink>(reused).Body(), old_body);
  EXPECT_EQ(scene.Get<Transform>(reused).position.x, -5.0F);
  EXPECT_EQ(scene.BodyCount(), 1U);
}

TEST(Physics, CopiedLinkGetsItsOwnBody) {
  PhysicsHarness scene;
  scene.AddFloor();
  const Entity original = scene.AddDynamic(At({-14.0F, 0.5F, -4.0F}));
  scene.Run(1);
  const physics::BodyHandle body = scene.Get<PhysicsLink>(original).Body();

  const Entity copy = scene.AddStatic(At({14.0F, 4.0F, 14.0F}));
  scene.Entities().AddComponent(copy, scene.Get<PhysicsLink>(original));
  scene.Run(2);

  EXPECT_EQ(scene.Get<PhysicsLink>(original).Body(), body);
  EXPECT_NE(scene.Get<PhysicsLink>(copy).Body(), body);
  EXPECT_NEAR(scene.Get<Transform>(original).position.x, -14.0F, 0.01F);
  EXPECT_EQ(scene.BodyCount(), scene.LinkCount());
}

TEST(Physics, ChurnLeaksNoBodies) {
  PhysicsHarness scene;
  scene.AddFloor();
  // More cycles than the world's capacity: a leak would run Jolt out of bodies
  for (int i = 0; i < 6000; ++i) {
    const Entity box = scene.AddDynamic(At({0.0F, 8.0F, 0.0F}));
    scene.Run(1);
    scene.Entities().DestroyEntity(box);
    scene.Entities().Flush();
  }
  scene.Run(1);
  EXPECT_EQ(scene.BodyCount(), 1U);
  EXPECT_EQ(scene.LinkCount(), 1U);
}

// --- Simulation ------------------------------------------------------------

TEST(Physics, DropReboundsWithRestitution) {
  PhysicsHarness scene;
  scene.AddFloor();
  // Bottom at y = 3; restitution 0.6 should rebound ~0.6^2 * 3 = 1.08 m
  const Entity box = scene.AddDynamic(At({-10.0F, 3.5F, -3.0F}));
  float peak = 0.0F;
  float previous_vy = 0.0F;
  bool hit = false;
  for (int i = 0; i < 180; ++i) {
    scene.Run(1);
    const float vy = scene.Get<RigidBody>(box).velocity.y;
    hit = hit || (previous_vy < 0.0F && vy > 0.0F);
    if (hit) {
      peak = std::max(peak, scene.Get<Transform>(box).position.y - 0.5F);
    }
    previous_vy = vy;
  }
  EXPECT_GT(peak, 0.8F);
  EXPECT_LT(peak, 1.2F);
}

TEST(Physics, OffCentreBoxRestsUprightOnItsBottom) {
  PhysicsHarness scene;
  scene.AddFloor();
  // Like the bunny: the shape sits above the entity's origin
  const bx::Aabb bounds{.min = {-0.5F, 0.0F, -0.5F}, .max = {0.5F, 1.5F, 0.5F}};
  const Entity box =
      scene.AddDynamic(At({0.0F, 3.0F, 0.0F}), BoxColliderAround(bounds));
  scene.Run(300);
  EXPECT_NEAR(scene.Get<Transform>(box).position.y, 0.0F, 0.03F);
  EXPECT_LT(bx::length(scene.Get<RigidBody>(box).velocity), 0.01F);
  EXPECT_GT(Upness(scene.Get<Transform>(box)), 0.999F);
}

TEST(Physics, FloorFrictionStopsASlide) {
  PhysicsHarness scene;
  scene.AddFloor();
  const Entity box = scene.AddDynamic(At({0.0F, 0.5F, 0.0F}));
  scene.Run(60);
  const float start_x = scene.Get<Transform>(box).position.x;
  scene.Get<RigidBody>(box).velocity = {-4.0F, 0.0F, 0.0F};
  scene.Run(240);
  // Friction 0.2 stops 4 m/s in about 4 m
  const float slid = start_x - scene.Get<Transform>(box).position.x;
  EXPECT_LT(bx::length(scene.Get<RigidBody>(box).velocity), 0.05F);
  EXPECT_GT(slid, 2.0F);
  EXPECT_LT(slid, 6.0F);
}

TEST(Physics, WallStopsABody) {
  PhysicsHarness scene;
  scene.AddFloor();
  scene.AddWallEast();
  const Entity box = scene.AddDynamic(At({0.0F, 0.5F, 0.0F}));
  scene.Run(60);
  scene.Get<RigidBody>(box).velocity = {10.0F, 0.0F, 0.0F};
  scene.Run(240);
  EXPECT_GT(scene.Get<Transform>(box).position.x, 15.0F);
  EXPECT_LT(scene.Get<Transform>(box).position.x, 19.75F);
}

TEST(Physics, AccelerationMovesABody) {
  PhysicsHarness scene;
  const Entity box = scene.AddDynamic(At({0.0F, 5.0F, 0.0F}));
  scene.Get<RigidBody>(box).has_gravity = false;
  scene.Get<RigidBody>(box).acceleration = {5.0F, 0.0F, 0.0F};
  scene.Run(1);
  const float start_x = scene.Get<Transform>(box).position.x;
  scene.Run(60);
  // 0.5 * 5 m/s^2 * 1 s^2
  EXPECT_NEAR(scene.Get<Transform>(box).position.x - start_x, 2.5F, 0.15F);
}

TEST(Physics, TiltedCubeTipsOntoAFace) {
  PhysicsHarness scene;
  scene.AddFloor();
  // Not 45 degrees: a cube landing exactly on its edge stays balanced
  const Entity cube = scene.AddDynamic(
      At({-3.0F, 2.0F, -3.0F}, EulerToQuat({0.0F, 0.0F, 0.5F})));
  scene.Run(300);
  EXPECT_GT(Upness(scene.Get<Transform>(cube)), 0.999F);
  EXPECT_NEAR(scene.Get<Transform>(cube).position.y, 0.5F, 0.03F);
}

// The collider must tilt the way the renderer draws: a box dropped off-centre
// lands on the ramp's rendered top face, not ~1 m above or below it
TEST(Physics, RampContactMatchesTheRenderedTilt) {
  PhysicsHarness scene;
  const Entity ramp = scene.AddRamp();
  const std::array<float, 16> mtx = ModelMatrix(scene.Get<Transform>(ramp));
  const bx::Vec3 top_point = bx::mulH({0.0F, 0.5F, 0.0F}, mtx.data());
  const bx::Vec3 up =
      bx::normalize(bx::sub(bx::mul({0.0F, 1.0F, 0.0F}, mtx.data()),
                            bx::mul({0.0F, 0.0F, 0.0F}, mtx.data())));
  const Entity box = scene.AddDynamic(At({0.0F, 3.5F, -16.0F},
                                         bx::Quaternion{bx::InitIdentity},
                                         {0.5F, 0.5F, 0.5F}));

  // It bounces, so track its closest approach to the surface
  float closest = 1e9F;
  for (int i = 0; i < 90; ++i) {
    scene.Run(1);
    closest = std::min(
        closest,
        bx::dot(bx::sub(scene.Get<Transform>(box).position, top_point), up));
  }
  EXPECT_NEAR(closest, 0.25F, 0.05F);
}

TEST(Physics, SphereRestsAtItsLargestScaledRadius) {
  PhysicsHarness scene;
  scene.AddFloor();
  // Spheres cannot stretch: radius 0.5 scaled by the largest axis, 2
  const Entity ball =
      scene.AddDynamic(At({-14.0F, 3.0F, 4.0F},
                          bx::Quaternion{bx::InitIdentity}, {1.0F, 2.0F, 1.0F}),
                       WithRestitution(Sphere(0.5F), 0.0F));
  scene.Run(240);
  EXPECT_NEAR(scene.Get<Transform>(ball).position.y, 1.0F, 0.03F);
}

TEST(Physics, SphereRollsDownTheRamp) {
  PhysicsHarness scene;
  scene.AddRamp(0.0F);
  const Entity ball = scene.AddDynamic(At({0.0F, 2.5F, -15.0F}),
                                       WithRestitution(Sphere(0.5F), 0.0F));
  scene.Run(1);
  const float start_z = scene.Get<Transform>(ball).position.z;

  // Sum per-frame turns: the total angle wraps past a full revolution
  float spin = 0.0F;
  bx::Quaternion last = scene.Get<Transform>(ball).rotation;
  for (int i = 0; i < 120; ++i) {
    scene.Run(1);
    const bx::Quaternion now = scene.Get<Transform>(ball).rotation;
    spin += 2.0F * std::acos(std::min(1.0F, std::abs(bx::dot(last, now))));
    last = now;
  }
  const float rolled =
      std::abs(scene.Get<Transform>(ball).position.z - start_z);
  EXPECT_GT(rolled, 1.0F);
  // Rolling without slipping turns distance / radius radians
  EXPECT_GT(spin, 0.8F * rolled / 0.5F);
}

// --- Edits made outside physics --------------------------------------------

TEST(Physics, PositionEditTeleportsTheBody) {
  PhysicsHarness scene;
  scene.AddFloor();
  const Entity box = scene.AddDynamic(At({0.0F, 0.5F, 0.0F}));
  scene.Run(60);
  scene.Get<Transform>(box).position = {0.0F, 5.0F, 0.0F};
  scene.Get<RigidBody>(box).velocity = {0.0F, 0.0F, 0.0F};
  scene.Run(1);
  EXPECT_GT(scene.Get<Transform>(box).position.y, 4.99F);
  EXPECT_LE(scene.Get<Transform>(box).position.y, 5.0F);
}

TEST(Physics, GravityToggleFloatsAndDropsTheBody) {
  PhysicsHarness scene;
  const Entity box = scene.AddDynamic(At({0.0F, 5.0F, 0.0F}));
  scene.Get<RigidBody>(box).has_gravity = false;
  scene.Run(120);
  const float floating_y = scene.Get<Transform>(box).position.y;
  EXPECT_FLOAT_EQ(floating_y, 5.0F);

  // Long enough to fall asleep first: the toggle must wake it
  scene.Get<RigidBody>(box).has_gravity = true;
  scene.Run(30);
  EXPECT_LT(scene.Get<Transform>(box).position.y, floating_y - 1.0F);
}

TEST(Physics, ScaleEditReshapesTheSameBody) {
  PhysicsHarness scene;
  scene.AddFloor();
  const Entity box = scene.AddDynamic(At({0.0F, 2.0F, 0.0F}),
                                      WithRestitution(Collider{}, 0.0F));
  scene.Run(180);
  const float rest_y = scene.Get<Transform>(box).position.y;
  const physics::BodyHandle body = scene.Get<PhysicsLink>(box).Body();

  scene.Get<Transform>(box).scale = {2.0F, 2.0F, 2.0F};
  scene.Run(180);
  EXPECT_EQ(scene.Get<PhysicsLink>(box).Body(), body);
  EXPECT_NEAR(scene.Get<Transform>(box).position.y, 2.0F * rest_y, 0.05F);
}

// --- Sleeping bodies wake when what they rest on changes -------------------

/** @brief A grippy small box asleep on the ramp. @return The box. */
auto SleepingBoxOnRamp(PhysicsHarness& scene, Entity ramp) -> Entity {
  scene.Get<Collider>(ramp).material.restitution = 0.0F;
  Collider grippy = WithRestitution(Collider{}, 0.0F);
  grippy.material.friction =
      10.0F;  // sqrt(10 * 0.2) = 1.4 > tan(15 deg): holds
  const Entity box =
      scene.AddDynamic(At({0.0F, 3.0F, -14.0F},
                          bx::Quaternion{bx::InitIdentity}, {0.5F, 0.5F, 0.5F}),
                       grippy);
  scene.Run(240);
  return box;
}

TEST(Physics, StaticFrictionEditWakesWhatRestsOnIt) {
  PhysicsHarness scene;
  const Entity ramp = scene.AddRamp();
  const Entity box = SleepingBoxOnRamp(scene, ramp);
  const bx::Vec3 rest = scene.Get<Transform>(box).position;
  scene.Run(30);
  ASSERT_LT(bx::distance(rest, scene.Get<Transform>(box).position), 1e-4F);

  scene.Get<Collider>(ramp).material.friction = 0.0F;
  scene.Run(30);
  EXPECT_GT(bx::distance(rest, scene.Get<Transform>(box).position), 0.05F);
}

TEST(Physics, DynamicFrictionEditWakesTheBody) {
  PhysicsHarness scene;
  const Entity ramp = scene.AddRamp();
  const Entity box = SleepingBoxOnRamp(scene, ramp);
  const bx::Vec3 rest = scene.Get<Transform>(box).position;

  scene.Get<Collider>(box).material.friction = 0.0F;
  scene.Run(30);
  EXPECT_GT(bx::distance(rest, scene.Get<Transform>(box).position), 0.05F);
}

/** @brief A box asleep on a static platform. @return The box. */
auto SleepingBoxOnPlatform(PhysicsHarness& scene, Entity& platform) -> Entity {
  platform =
      scene.AddStatic(At({12.0F, 2.0F, -12.0F},
                         bx::Quaternion{bx::InitIdentity}, {4.0F, 0.2F, 4.0F}),
                      WithRestitution(Collider{}, 0.0F));
  const Entity box =
      scene.AddDynamic(At({12.0F, 2.5F, -12.0F},
                          bx::Quaternion{bx::InitIdentity}, {0.5F, 0.5F, 0.5F}),
                       WithRestitution(Collider{}, 0.0F));
  scene.Run(240);
  return box;
}

TEST(Physics, LoweringASupportWakesWhatSleepsOnIt) {
  PhysicsHarness scene;
  Entity platform = 0;
  const Entity box = SleepingBoxOnPlatform(scene, platform);
  const float rest_y = scene.Get<Transform>(box).position.y;
  scene.Get<Transform>(platform).position.y -= 0.5F;
  scene.Run(30);
  EXPECT_GT(rest_y - scene.Get<Transform>(box).position.y, 0.4F);
}

TEST(Physics, ThinningASupportWakesWhatSleepsOnIt) {
  PhysicsHarness scene;
  Entity platform = 0;
  const Entity box = SleepingBoxOnPlatform(scene, platform);
  const float rest_y = scene.Get<Transform>(box).position.y;
  scene.Get<Transform>(platform).scale.y = 0.05F;
  scene.Run(30);
  // Boxes are at least 10 cm thick, so the top drops 5 cm, not 7.5
  EXPECT_GT(rest_y - scene.Get<Transform>(box).position.y, 0.02F);
}

TEST(Physics, DestroyingASupportWakesWhatSleepsOnIt) {
  PhysicsHarness scene;
  Entity platform = 0;
  const Entity box = SleepingBoxOnPlatform(scene, platform);
  const float rest_y = scene.Get<Transform>(box).position.y;
  scene.Entities().DestroyEntity(platform);
  scene.Entities().Flush();
  scene.Run(30);
  EXPECT_GT(rest_y - scene.Get<Transform>(box).position.y, 0.5F);
}

// --- Characters
// ----------------------------------------------------------------

TEST(Physics, CharacterBodyStandsOnTheFloor) {
  PhysicsHarness scene;
  scene.AddFloor();
  const Entity character = scene.AddCharacter({0.0F, 1.0F, 0.0F});
  scene.Run(120);
  EXPECT_EQ(scene.CharacterCount(), 1U);
  EXPECT_TRUE(scene.Get<CharacterLink>(character).OnGround());
  EXPECT_NEAR(scene.Get<Transform>(character).position.y, 0.0F, 0.05F);
}

TEST(Physics, CharacterVelocityEditMovesIt) {
  PhysicsHarness scene;
  scene.AddFloor();
  const Entity character = scene.AddCharacter({0.0F, 0.0F, 0.0F});
  scene.Run(30);
  scene.Get<CharacterBody>(character).velocity = {2.0F, 0.0F, 0.0F};
  scene.Run(60);
  // One second at 2 m/s
  EXPECT_NEAR(scene.Get<Transform>(character).position.x, 2.0F, 0.1F);
  EXPECT_NEAR(scene.Get<CharacterBody>(character).velocity.x, 2.0F, 1e-4F);
}

TEST(Physics, CharacterPositionEditTeleportsIt) {
  PhysicsHarness scene;
  scene.AddFloor();
  const Entity character = scene.AddCharacter({0.0F, 0.0F, 0.0F});
  scene.Run(30);
  scene.Get<Transform>(character).position = {5.0F, 0.0F, -3.0F};
  scene.Run(1);
  EXPECT_NEAR(scene.Get<Transform>(character).position.x, 5.0F, 1e-4F);
  EXPECT_NEAR(scene.Get<Transform>(character).position.z, -3.0F, 1e-4F);
}

TEST(Physics, CharacterRadiusEditRebuildsTheCapsule) {
  PhysicsHarness scene;
  scene.AddFloor();
  scene.AddWallEast();
  const Entity character = scene.AddCharacter({17.0F, 0.0F, 0.0F});
  scene.Get<CharacterBody>(character).velocity = {3.0F, 0.0F, 0.0F};
  scene.Run(120);
  // The wall's inner face is at x = 19.75
  EXPECT_NEAR(scene.Get<Transform>(character).position.x, 19.45F, 0.05F);

  scene.Get<CharacterBody>(character).radius = 0.6F;
  scene.Run(120);
  EXPECT_NEAR(scene.Get<Transform>(character).position.x, 19.15F, 0.05F);
  EXPECT_EQ(scene.CharacterCount(), 1U);
}

TEST(Physics, RemovingCharacterBodyDestroysTheCharacter) {
  PhysicsHarness scene;
  const Entity character = scene.AddCharacter({0.0F, 0.0F, 0.0F});
  scene.Run(1);
  ASSERT_EQ(scene.CharacterCount(), 1U);
  scene.Entities().RemoveComponent<CharacterBody>(character);
  scene.Run(1);
  EXPECT_EQ(scene.CharacterCount(), 0U);
  EXPECT_FALSE(scene.Entities().HasComponent<CharacterLink>(character));
}

TEST(Physics, DestroyedEntityLosesItsCharacter) {
  PhysicsHarness scene;
  const Entity character = scene.AddCharacter({0.0F, 0.0F, 0.0F});
  scene.Run(1);
  scene.Entities().DestroyEntity(character);
  scene.Entities().Flush();
  scene.Run(1);
  EXPECT_EQ(scene.CharacterCount(), 0U);
}

TEST(Physics, CopiedCharacterLinkGetsItsOwnCharacter) {
  PhysicsHarness scene;
  const Entity original = scene.AddCharacter({0.0F, 0.0F, 0.0F});
  scene.Run(1);
  const Entity copy = scene.AddCharacter({4.0F, 0.0F, 0.0F});
  scene.Entities().AddComponent(copy, scene.Get<CharacterLink>(original));
  scene.Run(1);
  EXPECT_EQ(scene.CharacterCount(), 2U);
  EXPECT_NE(scene.Get<CharacterLink>(copy).Character(),
            scene.Get<CharacterLink>(original).Character());
}

TEST(Physics, ColliderOnACharacterMakesNoBody) {
  PhysicsHarness scene;
  const Entity character = scene.AddCharacter({0.0F, 0.0F, 0.0F});
  scene.Entities().AddComponent(character, Collider{});
  scene.Run(1);
  EXPECT_EQ(scene.BodyCount(), 0U);
  EXPECT_EQ(scene.CharacterCount(), 1U);
}

// --- Mesh colliders ----------------------------------------------------------

TEST(Physics, MeshFloorHoldsWhatFallsOnIt) {
  PhysicsHarness scene;
  scene.AddStatic(At({0.0F, 0.0F, 0.0F}),
                  MeshCollider(scene.CreateCollisionMesh(FloorSquare(10.0F))));
  const Entity ball = scene.AddDynamic(At({0.0F, 3.0F, 0.0F}),
                                       WithRestitution(Sphere(0.5F), 0.0F));
  scene.Run(240);
  EXPECT_NEAR(scene.Get<Transform>(ball).position.y, 0.5F, 0.03F);
}

TEST(Physics, MeshFloorIsOneSided) {
  PhysicsHarness scene;
  scene.AddStatic(At({0.0F, 0.0F, 0.0F}),
                  MeshCollider(scene.CreateCollisionMesh(FloorSquare(10.0F))));
  // Rising from below, it meets the floor's back face
  const Entity ball = scene.AddDynamic(At({0.0F, -2.0F, 0.0F}), Sphere(0.5F));
  scene.Get<RigidBody>(ball).has_gravity = false;
  scene.Get<RigidBody>(ball).velocity = {0.0F, 5.0F, 0.0F};
  scene.Run(60);
  EXPECT_GT(scene.Get<Transform>(ball).position.y, 2.0F);
}

TEST(Physics, MeshFloorScalesWithItsTransform) {
  PhysicsHarness scene;
  // 2 m wide, scaled to 8 m on X and Z
  scene.AddStatic(At({0.0F, 0.0F, 0.0F}, bx::Quaternion{bx::InitIdentity},
                     {4.0F, 1.0F, 4.0F}),
                  MeshCollider(scene.CreateCollisionMesh(FloorSquare(1.0F))));
  const Entity ball = scene.AddDynamic(At({3.0F, 3.0F, -3.0F}),
                                       WithRestitution(Sphere(0.5F), 0.0F));
  scene.Run(240);
  EXPECT_NEAR(scene.Get<Transform>(ball).position.y, 0.5F, 0.03F);
}

TEST(Physics, BodiesShareACollisionMeshAtTheirOwnScales) {
  PhysicsHarness scene;
  const physics::CollisionMeshHandle mesh =
      scene.CreateCollisionMesh(FloorSquare(1.0F));
  scene.AddStatic(At({-10.0F, 0.0F, 0.0F}), MeshCollider(mesh));
  scene.AddStatic(At({10.0F, 0.0F, 0.0F}, bx::Quaternion{bx::InitIdentity},
                     {4.0F, 1.0F, 4.0F}),
                  MeshCollider(mesh));
  const Entity small = scene.AddDynamic(At({-10.0F, 3.0F, 0.0F}),
                                        WithRestitution(Sphere(0.5F), 0.0F));
  // Only the scaled copy reaches 3 m from its centre
  const Entity large = scene.AddDynamic(At({13.0F, 3.0F, 0.0F}),
                                        WithRestitution(Sphere(0.5F), 0.0F));
  scene.Run(240);
  EXPECT_NEAR(scene.Get<Transform>(small).position.y, 0.5F, 0.03F);
  EXPECT_NEAR(scene.Get<Transform>(large).position.y, 0.5F, 0.03F);
}

TEST(Physics, MeshColliderStaysStaticWithARigidBody) {
  PhysicsHarness scene;
  const Entity floor = scene.AddDynamic(
      At({0.0F, 0.0F, 0.0F}),
      MeshCollider(scene.CreateCollisionMesh(FloorSquare(10.0F))));
  scene.Run(30);
  EXPECT_EQ(scene.Get<Transform>(floor).position.y, 0.0F);

  // Turning static and back asks for a motion change it ignores
  scene.Entities().RemoveComponent<RigidBody>(floor);
  scene.Run(1);
  scene.Entities().AddComponent(floor, RigidBody{});
  scene.Run(30);
  EXPECT_EQ(scene.Get<Transform>(floor).position.y, 0.0F);
}

}  // namespace

}  // namespace engine

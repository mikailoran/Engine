// Signature matching: which entities a system tracks, and when that changes.

#include "ecs/core/ecs.h"

#include <gtest/gtest.h>

namespace {

struct Position {
  float x{0.0F};
};

struct Velocity {
  float dx{0.0F};
};

/// Systems carry no logic here: only System::entities is under test.
struct Mover : System {};
struct Renderer : System {};

} // namespace

TEST(SystemMatching, TracksAnEntityOnlyWhenEveryComponentIsPresent) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  ecs.RegisterComponent<Velocity>();

  auto &mover = ecs.RegisterSystem<Mover>();
  Signature signature;
  signature.set(ecs.GetComponentBit<Position>());
  signature.set(ecs.GetComponentBit<Velocity>());
  ecs.SetSystemSignature<Mover>(signature);

  const auto entity = ecs.CreateEntity();

  ecs.AddComponent(entity, Position{});
  EXPECT_FALSE(mover.entities.contains(entity)) << "matched on a partial set";

  ecs.AddComponent(entity, Velocity{});
  EXPECT_TRUE(mover.entities.contains(entity)) << "did not match once complete";
}

TEST(SystemMatching, RemovingARequiredComponentUntracksTheEntity) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  auto &mover = ecs.RegisterSystem<Mover>();
  Signature signature;
  signature.set(ecs.GetComponentBit<Position>());
  ecs.SetSystemSignature<Mover>(signature);

  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{});
  ASSERT_TRUE(mover.entities.contains(entity));

  ecs.RemoveComponent<Position>(entity);

  EXPECT_FALSE(mover.entities.contains(entity));
}

TEST(SystemMatching, ExtraComponentsDoNotPreventMatching) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  ecs.RegisterComponent<Velocity>();

  auto &mover = ecs.RegisterSystem<Mover>();
  Signature signature;
  signature.set(ecs.GetComponentBit<Position>());
  ecs.SetSystemSignature<Mover>(signature);

  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{});
  ecs.AddComponent(entity, Velocity{});

  EXPECT_TRUE(mover.entities.contains(entity));
}

TEST(SystemMatching, OverlappingSignaturesTrackIndependently) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  ecs.RegisterComponent<Velocity>();

  auto &mover = ecs.RegisterSystem<Mover>();
  auto &renderer = ecs.RegisterSystem<Renderer>();

  Signature moving;
  moving.set(ecs.GetComponentBit<Position>());
  moving.set(ecs.GetComponentBit<Velocity>());
  ecs.SetSystemSignature<Mover>(moving);

  Signature drawable;
  drawable.set(ecs.GetComponentBit<Position>());
  ecs.SetSystemSignature<Renderer>(drawable);

  const auto still = ecs.CreateEntity();
  ecs.AddComponent(still, Position{});

  const auto moving_entity = ecs.CreateEntity();
  ecs.AddComponent(moving_entity, Position{});
  ecs.AddComponent(moving_entity, Velocity{});

  EXPECT_FALSE(mover.entities.contains(still));
  EXPECT_TRUE(renderer.entities.contains(still));
  EXPECT_TRUE(mover.entities.contains(moving_entity));
  EXPECT_TRUE(renderer.entities.contains(moving_entity));
}

TEST(SystemMatching, SystemWithNoSignatureMatchesNothing) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  auto &mover = ecs.RegisterSystem<Mover>();

  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{});

  EXPECT_TRUE(mover.entities.empty());
}

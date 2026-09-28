// A flushed DestroyEntity must reach all three managers: components, systems,
// and the id pool. Deferral itself is covered in deferred_destruction_test.cpp;
// these tests flush immediately and only assert the teardown is complete.

#include "ecs/core/ecs.h"

#include <gtest/gtest.h>

namespace {

struct Position {
  float x{0.0F};
};

struct Health {
  int value{0};
};

struct Mover : System {};

} // namespace

TEST(Destruction, RemovesEveryComponentOfTheEntity) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  ecs.RegisterComponent<Health>();

  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{});
  ecs.AddComponent(entity, Health{});

  ecs.DestroyEntity(entity);
  ecs.Flush();

  EXPECT_FALSE(ecs.HasComponent<Position>(entity));
  EXPECT_FALSE(ecs.HasComponent<Health>(entity));
}

// The entity is absent from most component arrays, so EntityDestroyed has to
// tolerate that rather than assert on it.
TEST(Destruction, ToleratesComponentsTheEntityNeverHad) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  ecs.RegisterComponent<Health>();

  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{});

  ecs.DestroyEntity(entity);
  ecs.Flush();

  EXPECT_FALSE(ecs.HasComponent<Position>(entity));
}

TEST(Destruction, UntracksTheEntityFromEverySystem) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  auto &mover = ecs.RegisterSystem<Mover>();
  Signature signature;
  signature.set(ecs.GetComponentBit<Position>());
  ecs.SetSystemSignature<Mover>(signature);

  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{});
  ASSERT_TRUE(mover.entities.contains(entity));

  ecs.DestroyEntity(entity);
  ecs.Flush();

  EXPECT_FALSE(mover.entities.contains(entity));
}

TEST(Destruction, LeavesOtherEntitiesUntouched) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  auto &mover = ecs.RegisterSystem<Mover>();
  Signature signature;
  signature.set(ecs.GetComponentBit<Position>());
  ecs.SetSystemSignature<Mover>(signature);

  const auto first = ecs.CreateEntity();
  const auto middle = ecs.CreateEntity();
  const auto last = ecs.CreateEntity();
  ecs.AddComponent(first, Position{1.0F});
  ecs.AddComponent(middle, Position{2.0F});
  ecs.AddComponent(last, Position{3.0F});

  ecs.DestroyEntity(middle);
  ecs.Flush();

  EXPECT_EQ(mover.entities.size(), 2U);
  EXPECT_TRUE(mover.entities.contains(first));
  EXPECT_TRUE(mover.entities.contains(last));
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(first).x, 1.0F);
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(last).x, 3.0F);
}

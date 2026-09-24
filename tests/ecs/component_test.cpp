// Component storage: round-trip, mutation, and data integrity across removals.

#include "ecs/core/ecs.h"

#include <gtest/gtest.h>

namespace {

struct Position {
  float x{0.0F};
  float y{0.0F};
};

struct Health {
  int value{0};
};

} // namespace

TEST(ComponentStorage, AddThenGetReturnsTheStoredValue) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  const auto entity = ecs.CreateEntity();

  ecs.AddComponent(entity, Position{3.0F, 4.0F});

  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(entity).x, 3.0F);
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(entity).y, 4.0F);
}

TEST(ComponentStorage, MutationThroughTheReferenceIsVisible) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{});

  ecs.GetComponent<Position>(entity).x = 9.0F;

  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(entity).x, 9.0F);
}

TEST(ComponentStorage, HasComponentReflectsAddAndRemove) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  const auto entity = ecs.CreateEntity();

  EXPECT_FALSE(ecs.HasComponent<Position>(entity));

  ecs.AddComponent(entity, Position{});
  EXPECT_TRUE(ecs.HasComponent<Position>(entity));

  ecs.RemoveComponent<Position>(entity);
  EXPECT_FALSE(ecs.HasComponent<Position>(entity));
}

TEST(ComponentStorage, ComponentTypesAreIndependent) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  ecs.RegisterComponent<Health>();
  const auto entity = ecs.CreateEntity();

  ecs.AddComponent(entity, Position{});

  EXPECT_TRUE(ecs.HasComponent<Position>(entity));
  EXPECT_FALSE(ecs.HasComponent<Health>(entity));
}

// The storage swaps the removed element with the last one, so removing from the
// middle is the case that can corrupt its index bookkeeping.
TEST(ComponentStorage, RemovingTheMiddleLeavesNeighboursIntact) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  const auto first = ecs.CreateEntity();
  const auto middle = ecs.CreateEntity();
  const auto last = ecs.CreateEntity();
  ecs.AddComponent(first, Position{1.0F, 1.0F});
  ecs.AddComponent(middle, Position{2.0F, 2.0F});
  ecs.AddComponent(last, Position{3.0F, 3.0F});

  ecs.RemoveComponent<Position>(middle);

  EXPECT_FALSE(ecs.HasComponent<Position>(middle));
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(first).x, 1.0F);
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(last).x, 3.0F);
}

TEST(ComponentStorage, RemovingTheLastLeavesNeighboursIntact) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  const auto first = ecs.CreateEntity();
  const auto last = ecs.CreateEntity();
  ecs.AddComponent(first, Position{1.0F, 1.0F});
  ecs.AddComponent(last, Position{2.0F, 2.0F});

  ecs.RemoveComponent<Position>(last);

  EXPECT_FALSE(ecs.HasComponent<Position>(last));
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(first).x, 1.0F);
}

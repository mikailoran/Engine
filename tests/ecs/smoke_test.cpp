#include "ecs/core/ecs.h"

#include <gtest/gtest.h>

// TODO: Do these smoke tests have any use?

namespace {

/// Minimal component: an aggregate with no dependencies of its own.
struct Position {
  float x{0.0F};
  float y{0.0F};
};

} // namespace

TEST(EcsSmoke, CreatesAnEntityInRange) {
  Ecs ecs;

  const auto entity = ecs.CreateEntity();

  EXPECT_LT(entity, kMaxEntities);
}

TEST(EcsSmoke, StoresAndReadsBackAComponent) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{1.0F, 2.0F});

  const auto &position = ecs.GetComponent<Position>(entity);
  EXPECT_FLOAT_EQ(position.x, 1.0F);
  EXPECT_FLOAT_EQ(position.y, 2.0F);
}

TEST(EcsSmoke, AssignsTheFirstComponentBitZero) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  EXPECT_EQ(ecs.GetComponentBit<Position>(), 0U);
}

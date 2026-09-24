// Entity lifecycle as observed through the Ecs facade.

#include "ecs/core/ecs.h"

#include <gtest/gtest.h>

#include <set>

TEST(EntityLifecycle, CreateReturnsDistinctIds) {
  Ecs ecs;
  std::set<Entity> seen;

  for (int i = 0; i < 100; ++i) {
    EXPECT_TRUE(seen.insert(ecs.CreateEntity()).second)
        << "CreateEntity handed out the same id twice";
  }
}

TEST(EntityLifecycle, IdsStayInRange) {
  Ecs ecs;

  for (int i = 0; i < 100; ++i) {
    EXPECT_LT(ecs.CreateEntity(), MAX_ENTITIES);
  }
}

// The free pool is FIFO and pre-seeded with every id, so a destroyed id goes to
// the back of the queue rather than being handed straight back out. Asserting
// the property rather than a specific id keeps this valid if the pool changes.
TEST(EntityLifecycle, DestroyedIdIsNotImmediatelyReused) {
  Ecs ecs;
  const auto first = ecs.CreateEntity();
  const auto second = ecs.CreateEntity();

  ecs.DestroyEntity(second);
  const auto third = ecs.CreateEntity();

  EXPECT_NE(third, second);
  EXPECT_NE(third, first);
}

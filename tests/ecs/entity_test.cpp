// Entity lifecycle as observed through the Ecs facade.

#include <gtest/gtest.h>

#include <set>

#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"

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
    EXPECT_LT(ecs.CreateEntity(), kMaxEntities);
  }
}

// The free pool is FIFO and pre-seeded with every id, so a destroyed id goes to
// the back of the queue rather than being handed straight back out. Asserting
// the property rather than a specific id keeps this valid if the pool changes.
TEST(EntityLifecycle, DestroyedIdIsNotImmediatelyReused) {
  Ecs ecs;
  const auto entity = ecs.CreateEntity();

  ecs.DestroyEntity(entity);
  ecs.Flush();

  EXPECT_NE(ecs.CreateEntity(), entity);
}

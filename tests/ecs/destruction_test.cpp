// A flushed DestroyEntity must remove the entity's components, drop it from
// views, and free its id. Deferral itself is covered in
// deferred_destruction_test.cpp; these tests flush immediately and only assert
// the teardown is complete.

#include "ecs/core/ecs.h"

#include <gtest/gtest.h>

#include <set>

namespace {

struct Position {
  float x{0.0F};
};

struct Health {
  int value{0};
};

/** @brief Collects the entities a Position view visits. */
std::set<Entity> Visited(Ecs &ecs) {
  std::set<Entity> visited;
  ecs.View<Position>().ForEach(
      [&visited](Entity entity, Position &) { visited.insert(entity); });
  return visited;
}

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

TEST(Destruction, FreesTheIdForReuse) {
  Ecs ecs;
  const auto entity = ecs.CreateEntity();

  ecs.DestroyEntity(entity);
  ecs.Flush();

  // Every id must be available again, including the freed one
  std::set<Entity> created;
  for (EntityType i = 0; i < kMaxEntities; ++i) {
    created.insert(ecs.CreateEntity());
  }
  EXPECT_TRUE(created.contains(entity));
}

TEST(Destruction, ViewsNoLongerVisitTheEntity) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{});
  ASSERT_TRUE(Visited(ecs).contains(entity));

  ecs.DestroyEntity(entity);
  ecs.Flush();

  EXPECT_FALSE(Visited(ecs).contains(entity));
}

TEST(Destruction, LeavesOtherEntitiesUntouched) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  const auto first = ecs.CreateEntity();
  const auto middle = ecs.CreateEntity();
  const auto last = ecs.CreateEntity();
  ecs.AddComponent(first, Position{1.0F});
  ecs.AddComponent(middle, Position{2.0F});
  ecs.AddComponent(last, Position{3.0F});

  ecs.DestroyEntity(middle);
  ecs.Flush();

  EXPECT_EQ(Visited(ecs), (std::set<Entity>{first, last}));
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(first).x, 1.0F);
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(last).x, 3.0F);
}

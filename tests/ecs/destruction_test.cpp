// A flushed DestroyEntity must remove the entity's components, which is what
// drops it from views, and free its id. Deferral itself is covered in
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

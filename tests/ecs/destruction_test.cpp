// A flushed DestroyEntity must remove the entity's components, which is what
// drops it from views, and free its id. Deferral itself is covered in
// deferred_destruction_test.cpp; these tests flush immediately and only assert
// the teardown is complete.

#include <gtest/gtest.h>

#include <memory>
#include <set>

#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"

namespace engine {

namespace {

struct Position {
  float x{0.0F};
};

struct Health {
  int value{0};
};

/// Owns a shared resource, so its release shows in the use_count.
struct Owner {
  std::shared_ptr<int> resource;
};

}  // namespace

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

// RemoveData swaps the last element into each hole, so the survivor's data
// moves twice here and must still be found.
TEST(Destruction, LeavesTheRemainingEntitiesIntact) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  const auto first = ecs.CreateEntity();
  const auto middle = ecs.CreateEntity();
  const auto last = ecs.CreateEntity();
  ecs.AddComponent(first, Position{1.0F});
  ecs.AddComponent(middle, Position{2.0F});
  ecs.AddComponent(last, Position{3.0F});

  ecs.DestroyEntity(middle);
  ecs.DestroyEntity(first);
  ecs.Flush();

  EXPECT_FALSE(ecs.HasComponent<Position>(first));
  EXPECT_FALSE(ecs.HasComponent<Position>(middle));
  EXPECT_TRUE(ecs.HasComponent<Position>(last));
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(last).x, 3.0F);
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

TEST(Destruction, ReleasesWhatTheEntitysComponentsOwn) {
  Ecs ecs;
  ecs.RegisterComponent<Owner>();
  const auto entity = ecs.CreateEntity();
  const auto resource = std::make_shared<int>(1);
  ecs.AddComponent(entity, Owner{resource});

  ecs.DestroyEntity(entity);
  ecs.Flush();

  EXPECT_EQ(resource.use_count(), 1);
}

}  // namespace engine

// Deferral itself: when a queued destruction takes effect, that repeating or
// re-issuing a request is harmless, and that a system may destroy the entity it
// is iterating.

#include "ecs/core/ecs.h"

#include <gtest/gtest.h>

#include <set>
#include <vector>

namespace {

struct Position {
  float x{0.0F};
};

/// Destroys every entity it visits, from inside the view's ForEach.
struct SelfReaper {
  /** @brief Requests destruction of every entity with a Position. */
  void Update(Ecs &ecs) {
    ecs.View<Position>().ForEach(
        [&ecs](Entity entity, Position &) { ecs.DestroyEntity(entity); });
  }
};

/// Destroys a fixed entity that is not the one being visited.
struct NeighbourReaper {
  Entity victim{0};

  /** @brief Requests destruction of victim once per visited entity. */
  void Update(Ecs &ecs) {
    ecs.View<Position>().ForEach(
        [&ecs, this](Entity, Position &) { ecs.DestroyEntity(victim); });
  }
};

/** @brief Collects the entities a Position view visits. */
std::set<Entity> Visited(Ecs &ecs) {
  std::set<Entity> visited;
  ecs.View<Position>().ForEach(
      [&visited](Entity entity, Position &) { visited.insert(entity); });
  return visited;
}

} // namespace

TEST(DeferredDestruction, EntitySurvivesUntilFlush) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{1.0F});

  ecs.DestroyEntity(entity);

  EXPECT_TRUE(ecs.HasComponent<Position>(entity))
      << "the request alone must not tear anything down";
  EXPECT_TRUE(Visited(ecs).contains(entity));
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(entity).x, 1.0F);

  ecs.Flush();

  EXPECT_FALSE(ecs.HasComponent<Position>(entity));
  EXPECT_FALSE(Visited(ecs).contains(entity));
}

TEST(DeferredDestruction, FlushOnAnEmptyQueueIsANoOp) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{});

  ecs.Flush();
  ecs.Flush();

  EXPECT_TRUE(ecs.HasComponent<Position>(entity));
  EXPECT_TRUE(Visited(ecs).contains(entity));
}

TEST(DeferredDestruction, FlushClearsTheQueue) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{});

  ecs.DestroyEntity(entity);
  ecs.Flush();

  // A second flush must not replay the queue and recycle the id again. The
  // symptom of a replay is a duplicate in the free pool, so drain enough ids
  // to see one.
  ecs.Flush();

  std::set<Entity> seen;
  for (int i = 0; i < 64; ++i) {
    EXPECT_TRUE(seen.insert(ecs.CreateEntity()).second)
        << "an id was handed out twice, so it entered the free pool twice";
  }
}

// Two systems can independently decide to kill the same entity within one
// frame, so a repeated request has to collapse to a single destruction rather
// than pushing the id onto the free pool twice.
TEST(DeferredDestruction, RepeatedRequestsInOneFrameDestroyOnce) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{});

  ecs.DestroyEntity(entity);
  ecs.DestroyEntity(entity);
  ecs.DestroyEntity(entity);
  ecs.Flush();

  EXPECT_FALSE(Visited(ecs).contains(entity));

  std::set<Entity> seen{entity};
  for (int i = 0; i < 64; ++i) {
    EXPECT_TRUE(seen.insert(ecs.CreateEntity()).second)
        << "the id pool handed out a duplicate id";
  }
}

TEST(DeferredDestruction, RequestingAnAlreadyDestroyedEntityIsIgnored) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{});

  ecs.DestroyEntity(entity);
  ecs.Flush();

  ecs.DestroyEntity(entity);
  ecs.Flush();

  std::set<Entity> seen{entity};
  for (int i = 0; i < 64; ++i) {
    EXPECT_TRUE(seen.insert(ecs.CreateEntity()).second)
        << "the id pool handed out a duplicate id";
  }
}

// An id below kMaxEntities that was never created is simply not alive, so the
// request is dropped and living_entity_count_ never under-decrements.
TEST(DeferredDestruction, RequestingANeverCreatedEntityIsIgnored) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  const auto entity = ecs.CreateEntity();

  ecs.DestroyEntity(entity + 1);
  ecs.Flush();

  std::set<Entity> seen{entity};
  for (int i = 0; i < 64; ++i) {
    EXPECT_TRUE(seen.insert(ecs.CreateEntity()).second)
        << "a never-created id was recycled into the free pool";
  }
}

// The reason deferral exists: destroying immediately would swap-and-pop the
// component array ForEach is walking.
TEST(DeferredDestruction, SystemCanDestroyTheEntityItIsIterating) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  SelfReaper reaper;

  std::vector<Entity> entities;
  for (int i = 0; i < 16; ++i) {
    const auto entity = ecs.CreateEntity();
    ecs.AddComponent(entity, Position{});
    entities.push_back(entity);
  }
  reaper.Update(ecs);

  EXPECT_EQ(Visited(ecs).size(), 16U)
      << "the request alone must not tear anything down";

  ecs.Flush();

  EXPECT_TRUE(Visited(ecs).empty());
  for (const auto &entity : entities) {
    EXPECT_FALSE(ecs.HasComponent<Position>(entity));
  }
}

TEST(DeferredDestruction, SystemCanDestroyADifferentEntityMidIteration) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  NeighbourReaper reaper;

  const auto survivor = ecs.CreateEntity();
  const auto victim = ecs.CreateEntity();
  ecs.AddComponent(survivor, Position{1.0F});
  ecs.AddComponent(victim, Position{2.0F});
  reaper.victim = victim;

  reaper.Update(ecs);
  ecs.Flush();

  EXPECT_FALSE(Visited(ecs).contains(victim));
  EXPECT_TRUE(Visited(ecs).contains(survivor));
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(survivor).x, 1.0F);
}

// Destroying mid-iteration must not disturb the packed component array for the
// entities that remain -- RemoveData swaps the last element into the hole.
TEST(DeferredDestruction, PackedDataSurvivesADeferredDestroy) {
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

  EXPECT_EQ(Visited(ecs), std::set<Entity>{last});
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(last).x, 3.0F);
}

// Requests queued across separate frames must not leak into one another.
TEST(DeferredDestruction, QueueDoesNotCarryAcrossFlushes) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  const auto first = ecs.CreateEntity();
  const auto second = ecs.CreateEntity();
  ecs.AddComponent(first, Position{});
  ecs.AddComponent(second, Position{});

  ecs.DestroyEntity(first);
  ecs.Flush();
  ASSERT_FALSE(Visited(ecs).contains(first));
  ASSERT_TRUE(Visited(ecs).contains(second));

  ecs.Flush();

  EXPECT_TRUE(Visited(ecs).contains(second))
      << "the previous frame's request was replayed";
}

// Deferral itself: when a queued destruction takes effect, that repeating or
// re-issuing a request is harmless, and that a ForEach callback may destroy the
// entity it is visiting.

#include <gtest/gtest.h>

#include <cstddef>
#include <initializer_list>
#include <set>
#include <vector>

#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"

namespace {

struct Position {
  float x{0.0F};
};

/** @brief Collects the entities a Position view visits. */
auto Visited(Ecs& ecs) -> std::set<Entity> {
  std::set<Entity> visited;
  ecs.View<Position>().ForEach(
      [&visited](Entity entity, Position&) -> void { visited.insert(entity); });
  return visited;
}

/**
 * @brief Creates entities until every id is in use.
 * @param alive Entities already alive in @p ecs.
 * @return The ids handed out; fewer than requested means one repeated.
 */
auto FillPool(Ecs& ecs, std::size_t alive) -> std::set<Entity> {
  std::set<Entity> created;
  for (std::size_t i = alive; i < kMaxEntities; ++i) {
    created.insert(ecs.CreateEntity());
  }
  return created;
}

/**
 * @brief Expects the id pool to hold each free id exactly once.
 *
 * A duplicate only surfaces once the pool is drained, so this fills it, then
 * recycles two ids in turn so that freeing the duplicate itself can't hide it.
 * @param alive Entities already alive in @p ecs.
 */
void ExpectPoolHoldsEachFreeIdOnce(Ecs& ecs, std::size_t alive) {
  const auto created = FillPool(ecs, alive);
  ASSERT_EQ(created.size(), kMaxEntities - alive)
      << "an id was handed out twice";

  // With the pool empty, a freed id must be the next one handed out
  for (const Entity freed : {*created.begin(), *created.rbegin()}) {
    ecs.DestroyEntity(freed);
    ecs.Flush();
    EXPECT_EQ(ecs.CreateEntity(), freed) << "the pool held a stale id";
  }
}

}  // namespace

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

// A replayed request would destroy whichever entity has since reused the id.
TEST(DeferredDestruction, FlushDoesNotReplayEarlierRequests) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  const auto entity = ecs.CreateEntity();
  ecs.DestroyEntity(entity);
  ecs.Flush();

  ASSERT_TRUE(FillPool(ecs, 0).contains(entity)) << "the id was not reused";
  ecs.AddComponent(entity, Position{});
  ecs.Flush();

  EXPECT_TRUE(ecs.HasComponent<Position>(entity))
      << "the previous frame's request was replayed";
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
  ExpectPoolHoldsEachFreeIdOnce(ecs, 0);
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

  ExpectPoolHoldsEachFreeIdOnce(ecs, 0);
}

// An id below kMaxEntities that was never created is simply not alive, so the
// request is dropped rather than putting that id into the pool a second time.
TEST(DeferredDestruction, RequestingANeverCreatedEntityIsIgnored) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  const auto entity = ecs.CreateEntity();

  ecs.DestroyEntity(entity + 1);
  ecs.Flush();

  ExpectPoolHoldsEachFreeIdOnce(ecs, 1);
}

// The reason deferral exists: destroying immediately would swap-and-pop the
// component array ForEach is walking.
TEST(DeferredDestruction, ForEachCanDestroyTheVisitedEntity) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  std::vector<Entity> entities;
  for (int i = 0; i < 16; ++i) {
    const auto entity = ecs.CreateEntity();
    ecs.AddComponent(entity, Position{});
    entities.push_back(entity);
  }
  ecs.View<Position>().ForEach(
      [&ecs](Entity entity, Position&) -> void { ecs.DestroyEntity(entity); });

  EXPECT_EQ(Visited(ecs).size(), 16U)
      << "the request alone must not tear anything down";

  ecs.Flush();

  EXPECT_TRUE(Visited(ecs).empty());
  for (const auto& entity : entities) {
    EXPECT_FALSE(ecs.HasComponent<Position>(entity));
  }
}

TEST(DeferredDestruction, ForEachCanDestroyADifferentEntity) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  const auto survivor = ecs.CreateEntity();
  const auto victim = ecs.CreateEntity();
  ecs.AddComponent(survivor, Position{1.0F});
  ecs.AddComponent(victim, Position{2.0F});

  // Requested on every visit, including the survivor's
  ecs.View<Position>().ForEach(
      [&ecs, victim](Entity, Position&) -> void { ecs.DestroyEntity(victim); });
  ecs.Flush();

  EXPECT_FALSE(Visited(ecs).contains(victim));
  EXPECT_TRUE(Visited(ecs).contains(survivor));
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(survivor).x, 1.0F);
}

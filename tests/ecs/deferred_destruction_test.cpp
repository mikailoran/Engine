// Deferral itself: when a queued destruction takes effect, that repeating or
// re-issuing a request is harmless, and that a system may destroy the entity it
// is iterating -- the case that was undefined before the queue existed.

#include "ecs/core/ecs.h"

#include <gtest/gtest.h>

#include <set>
#include <vector>

namespace {

struct Position {
  float x{0.0F};
};

struct Mover : System {};

/// Destroys every entity it visits, from inside the range-for over entities.
struct SelfReaper : System {
  void Update(Ecs &ecs) {
    for (const auto &entity : entities) {
      ecs.DestroyEntity(entity);
    }
  }
};

/// Destroys a fixed entity that is not the one being visited.
struct NeighbourReaper : System {
  Entity victim{0};

  void Update(Ecs &ecs) {
    for (const auto &_ : entities) {
      ecs.DestroyEntity(victim);
    }
  }
};

/**
 * @brief Registers Position and a system matching on it.
 * @return Reference to the registered system, owned by @p ecs.
 */
template <class SystemClass> SystemClass &SetUpWorld(Ecs &ecs) {
  ecs.RegisterComponent<Position>();

  auto &system = ecs.RegisterSystem<SystemClass>();
  Signature signature;
  signature.set(ecs.GetComponentBit<Position>());
  ecs.SetSystemSignature<SystemClass>(signature);

  return system;
}

} // namespace

TEST(DeferredDestruction, EntitySurvivesUntilFlush) {
  Ecs ecs;
  auto &mover = SetUpWorld<Mover>(ecs);

  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{1.0F});

  ecs.DestroyEntity(entity);

  EXPECT_TRUE(ecs.HasComponent<Position>(entity))
      << "the request alone must not tear anything down";
  EXPECT_TRUE(mover.entities.contains(entity));
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(entity).x, 1.0F);

  ecs.Flush();

  EXPECT_FALSE(ecs.HasComponent<Position>(entity));
  EXPECT_FALSE(mover.entities.contains(entity));
}

TEST(DeferredDestruction, FlushOnAnEmptyQueueIsANoOp) {
  Ecs ecs;
  auto &mover = SetUpWorld<Mover>(ecs);

  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{});

  ecs.Flush();
  ecs.Flush();

  EXPECT_TRUE(ecs.HasComponent<Position>(entity));
  EXPECT_TRUE(mover.entities.contains(entity));
}

TEST(DeferredDestruction, FlushClearsTheQueue) {
  Ecs ecs;
  SetUpWorld<Mover>(ecs);

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
  auto &mover = SetUpWorld<Mover>(ecs);

  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{});

  ecs.DestroyEntity(entity);
  ecs.DestroyEntity(entity);
  ecs.DestroyEntity(entity);
  ecs.Flush();

  EXPECT_FALSE(mover.entities.contains(entity));

  std::set<Entity> seen{entity};
  for (int i = 0; i < 64; ++i) {
    EXPECT_TRUE(seen.insert(ecs.CreateEntity()).second)
        << "the id pool handed out a duplicate id";
  }
}

TEST(DeferredDestruction, RequestingAnAlreadyDestroyedEntityIsIgnored) {
  Ecs ecs;
  SetUpWorld<Mover>(ecs);

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

// An id below MAX_ENTITIES that was never created is simply not alive, so the
// request is dropped and living_entity_count_ never under-decrements.
TEST(DeferredDestruction, RequestingANeverCreatedEntityIsIgnored) {
  Ecs ecs;
  SetUpWorld<Mover>(ecs);

  const auto entity = ecs.CreateEntity();

  ecs.DestroyEntity(entity + 1);
  ecs.Flush();

  std::set<Entity> seen{entity};
  for (int i = 0; i < 64; ++i) {
    EXPECT_TRUE(seen.insert(ecs.CreateEntity()).second)
        << "a never-created id was recycled into the free pool";
  }
}

// The reason deferral exists. Destroying immediately from here erased from the
// std::set the range-for was holding, and the next ++ was undefined.
TEST(DeferredDestruction, SystemCanDestroyTheEntityItIsIterating) {
  Ecs ecs;
  auto &reaper = SetUpWorld<SelfReaper>(ecs);

  std::vector<Entity> entities;
  for (int i = 0; i < 16; ++i) {
    const auto entity = ecs.CreateEntity();
    ecs.AddComponent(entity, Position{});
    entities.push_back(entity);
  }
  ASSERT_EQ(reaper.entities.size(), 16U);

  reaper.Update(ecs);

  EXPECT_EQ(reaper.entities.size(), 16U)
      << "the set must not be mutated while it is being walked";

  ecs.Flush();

  EXPECT_TRUE(reaper.entities.empty());
  for (const auto &entity : entities) {
    EXPECT_FALSE(ecs.HasComponent<Position>(entity));
  }
}

TEST(DeferredDestruction, SystemCanDestroyADifferentEntityMidIteration) {
  Ecs ecs;
  auto &reaper = SetUpWorld<NeighbourReaper>(ecs);

  const auto survivor = ecs.CreateEntity();
  const auto victim = ecs.CreateEntity();
  ecs.AddComponent(survivor, Position{1.0F});
  ecs.AddComponent(victim, Position{2.0F});
  reaper.victim = victim;

  reaper.Update(ecs);
  ecs.Flush();

  EXPECT_FALSE(reaper.entities.contains(victim));
  EXPECT_TRUE(reaper.entities.contains(survivor));
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(survivor).x, 1.0F);
}

// Destroying mid-iteration must not disturb the packed component array for the
// entities that remain -- RemoveData swaps the last element into the hole.
TEST(DeferredDestruction, PackedDataSurvivesADeferredDestroy) {
  Ecs ecs;
  auto &mover = SetUpWorld<Mover>(ecs);

  const auto first = ecs.CreateEntity();
  const auto middle = ecs.CreateEntity();
  const auto last = ecs.CreateEntity();
  ecs.AddComponent(first, Position{1.0F});
  ecs.AddComponent(middle, Position{2.0F});
  ecs.AddComponent(last, Position{3.0F});

  ecs.DestroyEntity(middle);
  ecs.DestroyEntity(first);
  ecs.Flush();

  EXPECT_EQ(mover.entities.size(), 1U);
  EXPECT_TRUE(mover.entities.contains(last));
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(last).x, 3.0F);
}

// Requests queued across separate frames must not leak into one another.
TEST(DeferredDestruction, QueueDoesNotCarryAcrossFlushes) {
  Ecs ecs;
  auto &mover = SetUpWorld<Mover>(ecs);

  const auto first = ecs.CreateEntity();
  const auto second = ecs.CreateEntity();
  ecs.AddComponent(first, Position{});
  ecs.AddComponent(second, Position{});

  ecs.DestroyEntity(first);
  ecs.Flush();
  ASSERT_FALSE(mover.entities.contains(first));
  ASSERT_TRUE(mover.entities.contains(second));

  ecs.Flush();

  EXPECT_TRUE(mover.entities.contains(second))
      << "the previous frame's request was replayed";
}

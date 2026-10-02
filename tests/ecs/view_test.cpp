// Views: which entities ForEach visits, and what it hands to the callback.

#include "ecs/core/ecs.h"

#include <gtest/gtest.h>

#include <set>
#include <vector>

namespace {

struct Position {
  float x{0.0F};
};

struct Velocity {
  float dx{0.0F};
};

/**
 * @brief Collects the entities a view visits.
 * @return Visited entities in id order; repeat visits are kept.
 */
template <class... Components> std::multiset<Entity> Visited(Ecs &ecs) {
  std::multiset<Entity> visited;
  ecs.View<Components...>().ForEach(
      [&visited](Entity entity, Components &...) { visited.insert(entity); });
  return visited;
}

/** @brief Registers Position and Velocity in that order. */
void RegisterBoth(Ecs &ecs) {
  ecs.RegisterComponent<Position>();
  ecs.RegisterComponent<Velocity>();
}

} // namespace

TEST(View, VisitsAnEntityOnlyWhenEveryComponentIsPresent) {
  Ecs ecs;
  RegisterBoth(ecs);
  const auto entity = ecs.CreateEntity();

  ecs.AddComponent(entity, Position{});
  EXPECT_TRUE((Visited<Position, Velocity>(ecs).empty()))
      << "matched on a partial set";

  ecs.AddComponent(entity, Velocity{});
  EXPECT_EQ((Visited<Position, Velocity>(ecs)), std::multiset<Entity>{entity})
      << "did not match once complete";
}

TEST(View, RemovingARequiredComponentStopsVisits) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{});
  ASSERT_EQ(Visited<Position>(ecs), std::multiset<Entity>{entity});

  ecs.RemoveComponent<Position>(entity);

  EXPECT_TRUE(Visited<Position>(ecs).empty());
}

TEST(View, ExtraComponentsDoNotPreventMatching) {
  Ecs ecs;
  RegisterBoth(ecs);
  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{});
  ecs.AddComponent(entity, Velocity{});

  EXPECT_EQ(Visited<Position>(ecs), std::multiset<Entity>{entity});
}

TEST(View, OverlappingViewsMatchIndependently) {
  Ecs ecs;
  RegisterBoth(ecs);

  const auto still = ecs.CreateEntity();
  ecs.AddComponent(still, Position{});

  const auto moving = ecs.CreateEntity();
  ecs.AddComponent(moving, Position{});
  ecs.AddComponent(moving, Velocity{});

  EXPECT_EQ((Visited<Position, Velocity>(ecs)), std::multiset<Entity>{moving});
  EXPECT_EQ(Visited<Position>(ecs), (std::multiset<Entity>{still, moving}));
}

TEST(View, EmptyComponentArrayVisitsNothing) {
  Ecs ecs;
  RegisterBoth(ecs);
  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{});

  EXPECT_TRUE(Visited<Velocity>(ecs).empty());
  EXPECT_TRUE((Visited<Position, Velocity>(ecs).empty()));
}

// Velocity is the smaller array here, so it drives; Position is only checked.
TEST(View, VisitsMatchesWhicheverArrayDrives) {
  Ecs ecs;
  RegisterBoth(ecs);
  std::multiset<Entity> expected;
  for (int i = 0; i < 6; ++i) {
    const auto entity = ecs.CreateEntity();
    ecs.AddComponent(entity, Position{});
    if (i % 3 == 0) {
      ecs.AddComponent(entity, Velocity{});
      expected.insert(entity);
    }
  }
  const auto lone_velocity = ecs.CreateEntity();
  ecs.AddComponent(lone_velocity, Velocity{});

  EXPECT_EQ((Visited<Position, Velocity>(ecs)), expected);
  EXPECT_EQ((Visited<Velocity, Position>(ecs)), expected)
      << "the result depends on the order of the view's types";
}

// Every array holds every entity, so walking more than the driver repeats them.
TEST(View, VisitsEachMatchingEntityOnce) {
  Ecs ecs;
  RegisterBoth(ecs);
  std::multiset<Entity> expected;
  for (int i = 0; i < 3; ++i) {
    const auto entity = ecs.CreateEntity();
    ecs.AddComponent(entity, Position{});
    ecs.AddComponent(entity, Velocity{});
    expected.insert(entity);
  }

  EXPECT_EQ((Visited<Position, Velocity>(ecs)), expected);
}

TEST(View, CallbackReferencesMutateStoredComponents) {
  Ecs ecs;
  RegisterBoth(ecs);
  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{1.0F});
  ecs.AddComponent(entity, Velocity{2.0F});

  ecs.View<Position, Velocity>().ForEach(
      [](Entity, Position &position, Velocity &velocity) {
        position.x += velocity.dx;
      });

  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(entity).x, 3.0F);
}

TEST(View, CallbackReceivesEachEntitysOwnComponents) {
  Ecs ecs;
  RegisterBoth(ecs);
  std::vector<Entity> entities;
  for (int i = 0; i < 4; ++i) {
    const auto entity = ecs.CreateEntity();
    ecs.AddComponent(entity, Position{static_cast<float>(entity)});
    entities.push_back(entity);
  }
  // Swap-and-pop puts entity ids and dense slots out of step
  ecs.RemoveComponent<Position>(entities.at(1));

  int visits = 0;
  ecs.View<Position>().ForEach([&visits](Entity entity, Position &position) {
    EXPECT_FLOAT_EQ(position.x, static_cast<float>(entity));
    ++visits;
  });
  EXPECT_EQ(visits, 3);
}

// Two entities, so the loop reaches the check after the first callback adds.
TEST(ViewDeathTest, AddingAViewedComponentDuringForEachAsserts) {
#ifdef NDEBUG
  GTEST_SKIP() << "asserts are compiled out";
#else
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  ecs.AddComponent(ecs.CreateEntity(), Position{});
  ecs.AddComponent(ecs.CreateEntity(), Position{});

  EXPECT_DEATH(ecs.View<Position>().ForEach([&ecs](Entity, Position &) {
    ecs.AddComponent(ecs.CreateEntity(), Position{});
  }),
               "added or removed during ForEach");
#endif
}

// Component storage: round-trip, mutation, and data integrity across removals.

#include "ecs/core/ecs.h"

#include <gtest/gtest.h>

#include <set>
#include <stdexcept>
#include <string>

namespace {

struct Position {
  float x{0.0F};
  float y{0.0F};
};

struct Health {
  int value{0};
};

/// Non-trivial component: std::string has real move and swap semantics.
struct Name {
  std::string value;
};

/** @brief Collects the entities a @p Component view visits. */
template <class Component> std::set<Entity> Visited(Ecs &ecs) {
  std::set<Entity> visited;
  ecs.View<Component>().ForEach(
      [&visited](Entity entity, Component &) { visited.insert(entity); });
  return visited;
}

} // namespace

TEST(ComponentStorage, AddThenGetReturnsTheStoredValue) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  const auto entity = ecs.CreateEntity();

  ecs.AddComponent(entity, Position{3.0F, 4.0F});

  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(entity).x, 3.0F);
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(entity).y, 4.0F);
}

TEST(ComponentStorage, GetThroughAConstEcsReturnsTheStoredValue) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{3.0F, 4.0F});

  const Ecs &read_only = ecs;

  EXPECT_FLOAT_EQ(read_only.GetComponent<Position>(entity).x, 3.0F);
  EXPECT_FLOAT_EQ(read_only.GetComponent<Position>(entity).y, 4.0F);
}

TEST(ComponentStorage, MutationThroughTheReferenceIsVisible) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{});

  ecs.GetComponent<Position>(entity).x = 9.0F;

  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(entity).x, 9.0F);
}

TEST(ComponentStorage, HasComponentReflectsAddAndRemove) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  const auto entity = ecs.CreateEntity();

  EXPECT_FALSE(ecs.HasComponent<Position>(entity));

  ecs.AddComponent(entity, Position{});
  EXPECT_TRUE(ecs.HasComponent<Position>(entity));

  ecs.RemoveComponent<Position>(entity);
  EXPECT_FALSE(ecs.HasComponent<Position>(entity));
}

TEST(ComponentStorage, ComponentTypesAreIndependent) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  ecs.RegisterComponent<Health>();
  const auto entity = ecs.CreateEntity();

  ecs.AddComponent(entity, Position{});

  EXPECT_TRUE(ecs.HasComponent<Position>(entity));
  EXPECT_FALSE(ecs.HasComponent<Health>(entity));
}

// Ids that differ from their dense slot catch lookups that mix the two up.
TEST(ComponentStorage, GetFindsComponentsWhoseIdDiffersFromTheirSlot) {
  Ecs ecs;
  ecs.RegisterComponent<Health>();
  const auto first = ecs.CreateEntity();
  const auto second = ecs.CreateEntity();

  ecs.AddComponent(second, Health{20});
  ecs.AddComponent(first, Health{10});

  EXPECT_EQ(ecs.GetComponent<Health>(first).value, 10);
  EXPECT_EQ(ecs.GetComponent<Health>(second).value, 20);
}

// The storage swaps the removed element with the last one, so removing from the
// middle is the case that can corrupt its index bookkeeping.
TEST(ComponentStorage, RemovingTheMiddleLeavesTheOthersIntact) {
  Ecs ecs;
  ecs.RegisterComponent<Health>();
  const auto a = ecs.CreateEntity();
  const auto b = ecs.CreateEntity();
  const auto c = ecs.CreateEntity();
  const auto d = ecs.CreateEntity();
  ecs.AddComponent(a, Health{1});
  ecs.AddComponent(b, Health{2});
  ecs.AddComponent(c, Health{3});
  ecs.AddComponent(d, Health{4});

  ecs.RemoveComponent<Health>(b);

  EXPECT_FALSE(ecs.HasComponent<Health>(b));
  EXPECT_EQ(Visited<Health>(ecs), (std::set<Entity>{a, c, d}));
  EXPECT_EQ(ecs.GetComponent<Health>(a).value, 1);
  EXPECT_EQ(ecs.GetComponent<Health>(c).value, 3);
  EXPECT_EQ(ecs.GetComponent<Health>(d).value, 4);
}

// Each removal moves a survivor into the hole: first the heap-allocated name,
// then the small-string one, since the two move differently.
TEST(ComponentStorage, RemovalsKeepNonTrivialComponentsIntact) {
  Ecs ecs;
  ecs.RegisterComponent<Name>();
  const auto first = ecs.CreateEntity();
  const auto second = ecs.CreateEntity();
  const auto short_named = ecs.CreateEntity();
  const auto long_named = ecs.CreateEntity();
  const std::string long_name(64, 'x');
  ecs.AddComponent(first, Name{"first"});
  ecs.AddComponent(second, Name{"second"});
  ecs.AddComponent(short_named, Name{"short"});
  ecs.AddComponent(long_named, Name{long_name});

  ecs.RemoveComponent<Name>(second);
  ecs.RemoveComponent<Name>(first);

  EXPECT_EQ(Visited<Name>(ecs), (std::set<Entity>{short_named, long_named}));
  EXPECT_EQ(ecs.GetComponent<Name>(short_named).value, "short");
  EXPECT_EQ(ecs.GetComponent<Name>(long_named).value, long_name);
}

// The removed element is its own swap partner here.
TEST(ComponentStorage, RemovingTheLastLeavesTheOthersIntact) {
  Ecs ecs;
  ecs.RegisterComponent<Health>();
  const auto first = ecs.CreateEntity();
  const auto second = ecs.CreateEntity();
  const auto last = ecs.CreateEntity();
  ecs.AddComponent(first, Health{1});
  ecs.AddComponent(second, Health{2});
  ecs.AddComponent(last, Health{3});

  ecs.RemoveComponent<Health>(last);

  EXPECT_FALSE(ecs.HasComponent<Health>(last));
  EXPECT_EQ(Visited<Health>(ecs), (std::set<Entity>{first, second}));
  EXPECT_EQ(ecs.GetComponent<Health>(first).value, 1);
  EXPECT_EQ(ecs.GetComponent<Health>(second).value, 2);
}

TEST(ComponentStorage, RemovingTheOnlyComponentEmptiesTheStorage) {
  Ecs ecs;
  ecs.RegisterComponent<Health>();
  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Health{5});

  ecs.RemoveComponent<Health>(entity);

  EXPECT_FALSE(ecs.HasComponent<Health>(entity));
  EXPECT_TRUE(Visited<Health>(ecs).empty());
}

TEST(ComponentStorage, AComponentCanBeReaddedAfterRemoval) {
  Ecs ecs;
  ecs.RegisterComponent<Health>();
  const auto first = ecs.CreateEntity();
  const auto second = ecs.CreateEntity();
  ecs.AddComponent(first, Health{10});
  ecs.AddComponent(second, Health{20});
  ecs.RemoveComponent<Health>(first);

  ecs.AddComponent(first, Health{11});

  EXPECT_EQ(ecs.GetComponent<Health>(first).value, 11);
  EXPECT_EQ(ecs.GetComponent<Health>(second).value, 20);
  EXPECT_EQ(Visited<Health>(ecs), (std::set<Entity>{first, second}));
}

TEST(ComponentStorage, HasComponentThrowsForAnIdOutOfRange) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  EXPECT_THROW(static_cast<void>(ecs.HasComponent<Position>(kMaxEntities)),
               std::out_of_range);
}

// Component storage: round-trip, mutation, and data integrity across removals.

#include "ecs/core/ecs.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

namespace {

struct Position {
  float x{0.0F};
  float y{0.0F};
};

struct Health {
  int value{0};
};

/**
 * @brief Lists the entities with @p Component in the order a view visits them.
 * @return Dense order, which swap-and-pop makes differ from id order.
 */
template <class Component> std::vector<Entity> VisitOrder(Ecs &ecs) {
  std::vector<Entity> order;
  ecs.View<Component>().ForEach(
      [&order](Entity entity, Component &) { order.push_back(entity); });
  return order;
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

// The storage swaps the removed element with the last one, so removing from the
// middle is the case that can corrupt its index bookkeeping.
TEST(ComponentStorage, RemovingTheMiddleLeavesNeighboursIntact) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  const auto first = ecs.CreateEntity();
  const auto middle = ecs.CreateEntity();
  const auto last = ecs.CreateEntity();
  ecs.AddComponent(first, Position{1.0F, 1.0F});
  ecs.AddComponent(middle, Position{2.0F, 2.0F});
  ecs.AddComponent(last, Position{3.0F, 3.0F});

  ecs.RemoveComponent<Position>(middle);

  EXPECT_FALSE(ecs.HasComponent<Position>(middle));
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(first).x, 1.0F);
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(last).x, 3.0F);
}

TEST(ComponentStorage, RemovingTheLastLeavesNeighboursIntact) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  const auto first = ecs.CreateEntity();
  const auto last = ecs.CreateEntity();
  ecs.AddComponent(first, Position{1.0F, 1.0F});
  ecs.AddComponent(last, Position{2.0F, 2.0F});

  ecs.RemoveComponent<Position>(last);

  EXPECT_FALSE(ecs.HasComponent<Position>(last));
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(first).x, 1.0F);
}

// Dense order: views visit in the order components were added, not by id.

TEST(ComponentStorage, ViewsVisitInAdditionOrder) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  const auto first = ecs.CreateEntity();
  const auto second = ecs.CreateEntity();
  const auto third = ecs.CreateEntity();

  ecs.AddComponent(third, Position{});
  ecs.AddComponent(first, Position{});
  ecs.AddComponent(second, Position{});

  EXPECT_EQ(VisitOrder<Position>(ecs),
            (std::vector<Entity>{third, first, second}));
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

TEST(ComponentStorage, RemovingTheMiddleMovesTheLastIntoTheHole) {
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

  EXPECT_EQ(VisitOrder<Health>(ecs), (std::vector<Entity>{a, d, c}));
  EXPECT_EQ(ecs.GetComponent<Health>(d).value, 4);
  EXPECT_EQ(ecs.GetComponent<Health>(c).value, 3);
}

// The removed element is its own swap partner here.
TEST(ComponentStorage, RemovingTheLastKeepsTheOrderOfOthers) {
  Ecs ecs;
  ecs.RegisterComponent<Health>();
  const auto first = ecs.CreateEntity();
  const auto second = ecs.CreateEntity();
  const auto last = ecs.CreateEntity();
  ecs.AddComponent(first, Health{1});
  ecs.AddComponent(second, Health{2});
  ecs.AddComponent(last, Health{3});

  ecs.RemoveComponent<Health>(last);

  EXPECT_EQ(VisitOrder<Health>(ecs), (std::vector<Entity>{first, second}));
}

TEST(ComponentStorage, RemovingTheOnlyComponentEmptiesTheStorage) {
  Ecs ecs;
  ecs.RegisterComponent<Health>();
  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Health{5});

  ecs.RemoveComponent<Health>(entity);

  EXPECT_FALSE(ecs.HasComponent<Health>(entity));
  EXPECT_TRUE(VisitOrder<Health>(ecs).empty());
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
  EXPECT_EQ(VisitOrder<Health>(ecs), (std::vector<Entity>{second, first}));
}

TEST(ComponentStorage, HasComponentThrowsForAnIdOutOfRange) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  EXPECT_THROW(static_cast<void>(ecs.HasComponent<Position>(kMaxEntities)),
               std::out_of_range);
}

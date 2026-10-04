// Component storage: round-trip, mutation, data integrity across removals, and
// release of what a removed component owns.

#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <stdexcept>
#include <string>

#include "ecs/core/component_array.h"
#include "ecs/core/ecs.h"
#include "ecs/core/types.h"

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

/// Owns a shared resource, so its release shows in the use_count.
struct Owner {
  std::shared_ptr<int> resource;
};

/// Counts self-move-assignments, which leave many types unspecified.
class SelfMoveCounter {
 public:
  /** @brief Starts with no counter attached. */
  SelfMoveCounter() = default;

  /** @brief Reports self-moves to @p counter, which must outlive this. */
  explicit SelfMoveCounter(int* counter) : self_moves_(counter) {}

  ~SelfMoveCounter() = default;
  SelfMoveCounter(const SelfMoveCounter&) = default;
  auto operator=(const SelfMoveCounter&) -> SelfMoveCounter& = default;
  SelfMoveCounter(SelfMoveCounter&&) noexcept = default;

  /** @brief Moves @p other's counter in, counting a move onto itself. */
  auto operator=(SelfMoveCounter&& other) noexcept -> SelfMoveCounter& {
    if (this == &other && self_moves_ != nullptr) {
      ++*self_moves_;
    }
    self_moves_ = other.self_moves_;
    return *this;
  }

 private:
  int* self_moves_{nullptr};
};

/// Not storable: has no default constructor.
struct NoDefault {
  /** @brief Requires an argument, so cannot be default-constructed. */
  explicit NoDefault(int /*initial*/) {}
};

/// Not storable: can be neither copied nor moved.
class NotMovable {
 public:
  NotMovable() = default;
  ~NotMovable() = default;
  NotMovable(const NotMovable&) = delete;
  auto operator=(const NotMovable&) -> NotMovable& = delete;
  NotMovable(NotMovable&&) = delete;
  auto operator=(NotMovable&&) -> NotMovable& = delete;
};

// What ComponentArray accepts, checked at compile time
static_assert(ComponentType<Position>);
static_assert(ComponentType<Name>);
static_assert(ComponentType<Owner>);
static_assert(ComponentType<SelfMoveCounter>);
static_assert(!ComponentType<NoDefault>);
static_assert(!ComponentType<NotMovable>);

/** @brief Collects the entities a @p Component view visits. */
template <class Component>
auto Visited(Ecs& ecs) -> std::set<Entity> {
  std::set<Entity> visited;
  ecs.View<Component>().ForEach([&visited](Entity entity, Component&) -> auto {
    visited.insert(entity);
  });
  return visited;
}

}  // namespace

TEST(ComponentStorage, AddThenGetReturnsTheStoredValue) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  const auto entity = ecs.CreateEntity();

  ecs.AddComponent(entity, Position{.x = 3.0F, .y = 4.0F});

  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(entity).x, 3.0F);
  EXPECT_FLOAT_EQ(ecs.GetComponent<Position>(entity).y, 4.0F);
}

TEST(ComponentStorage, GetThroughAConstEcsReturnsTheStoredValue) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{.x = 3.0F, .y = 4.0F});

  const Ecs& read_only = ecs;

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

TEST(ComponentStorage, TryGetReturnsTheStoredComponent) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Position{.x = 3.0F, .y = 4.0F});

  auto* position = ecs.TryGetComponent<Position>(entity);

  ASSERT_NE(position, nullptr);
  EXPECT_EQ(position, &ecs.GetComponent<Position>(entity));
  EXPECT_FLOAT_EQ(position->x, 3.0F);
  EXPECT_FLOAT_EQ(position->y, 4.0F);
}

TEST(ComponentStorage, TryGetReturnsNullWithoutTheComponent) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();
  ecs.RegisterComponent<Health>();
  const auto entity = ecs.CreateEntity();

  EXPECT_EQ(ecs.TryGetComponent<Position>(entity), nullptr);

  ecs.AddComponent(entity, Position{});
  EXPECT_EQ(ecs.TryGetComponent<Health>(entity), nullptr);

  ecs.RemoveComponent<Position>(entity);
  EXPECT_EQ(ecs.TryGetComponent<Position>(entity), nullptr);
}

TEST(ComponentStorage, TryGetThrowsForAnIdOutOfRange) {
  Ecs ecs;
  ecs.RegisterComponent<Position>();

  EXPECT_THROW(static_cast<void>(ecs.TryGetComponent<Position>(kMaxEntities)),
               std::out_of_range);
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

TEST(ComponentStorage, RemovingTheOnlyComponentReleasesWhatItOwns) {
  Ecs ecs;
  ecs.RegisterComponent<Owner>();
  const auto entity = ecs.CreateEntity();
  const auto resource = std::make_shared<int>(1);
  ecs.AddComponent(entity, Owner{resource});

  ecs.RemoveComponent<Owner>(entity);

  EXPECT_EQ(resource.use_count(), 1);
}

// The hole is filled by the last element, which must not keep the removed
// component's resource alive in the vacated slot.
TEST(ComponentStorage, RemovingTheMiddleReleasesOnlyWhatItOwns) {
  Ecs ecs;
  ecs.RegisterComponent<Owner>();
  const auto first = ecs.CreateEntity();
  const auto middle = ecs.CreateEntity();
  const auto last = ecs.CreateEntity();
  const auto first_resource = std::make_shared<int>(1);
  const auto middle_resource = std::make_shared<int>(2);
  const auto last_resource = std::make_shared<int>(3);
  ecs.AddComponent(first, Owner{first_resource});
  ecs.AddComponent(middle, Owner{middle_resource});
  ecs.AddComponent(last, Owner{last_resource});

  ecs.RemoveComponent<Owner>(middle);

  EXPECT_EQ(middle_resource.use_count(), 1);
  EXPECT_EQ(first_resource.use_count(), 2);
  EXPECT_EQ(last_resource.use_count(), 2);
  EXPECT_EQ(ecs.GetComponent<Owner>(first).resource, first_resource);
  EXPECT_EQ(ecs.GetComponent<Owner>(last).resource, last_resource);
}

// Removing the last element makes it its own swap partner.
TEST(ComponentStorage, RemovalNeverSelfMoveAssignsAComponent) {
  Ecs ecs;
  ecs.RegisterComponent<SelfMoveCounter>();
  const auto entity = ecs.CreateEntity();
  int self_moves = 0;
  ecs.AddComponent(entity, SelfMoveCounter{&self_moves});

  ecs.RemoveComponent<SelfMoveCounter>(entity);

  EXPECT_EQ(self_moves, 0);
}

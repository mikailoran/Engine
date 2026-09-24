// Component bit assignment. These properties are what make a fresh Ecs per test
// safe, so they are pinned rather than assumed.

#include "ecs/core/ecs.h"

#include <gtest/gtest.h>

namespace {

struct Alpha {
  int value{0};
};

struct Beta {
  int value{0};
};

struct Gamma {
  int value{0};
};

} // namespace

TEST(Registration, BitsFollowRegistrationOrder) {
  Ecs ecs;
  ecs.RegisterComponent<Alpha>();
  ecs.RegisterComponent<Beta>();
  ecs.RegisterComponent<Gamma>();

  EXPECT_EQ(ecs.GetComponentBit<Alpha>(), 0U);
  EXPECT_EQ(ecs.GetComponentBit<Beta>(), 1U);
  EXPECT_EQ(ecs.GetComponentBit<Gamma>(), 2U);
}

// Bits are assigned per ComponentManager, not from a process-global counter.
// Without this, every test in the binary would share one 32-bit budget.
TEST(Registration, EachWorldAssignsBitsFromZero) {
  Ecs first;
  first.RegisterComponent<Alpha>();
  first.RegisterComponent<Beta>();

  Ecs second;
  second.RegisterComponent<Gamma>();

  EXPECT_EQ(first.GetComponentBit<Alpha>(), 0U);
  EXPECT_EQ(second.GetComponentBit<Gamma>(), 0U);
}

// The bit identifies (type, world), not the type alone.
TEST(Registration, SameTypeCanHoldDifferentBitsInDifferentWorlds) {
  Ecs first;
  first.RegisterComponent<Alpha>();
  first.RegisterComponent<Beta>();

  Ecs second;
  second.RegisterComponent<Beta>();
  second.RegisterComponent<Alpha>();

  EXPECT_EQ(first.GetComponentBit<Alpha>(), 0U);
  EXPECT_EQ(second.GetComponentBit<Alpha>(), 1U);
}

TEST(Registration, SignaturesFromDifferentWorldsDoNotAlias) {
  Ecs first;
  first.RegisterComponent<Alpha>();
  first.RegisterComponent<Beta>();

  Ecs second;
  second.RegisterComponent<Beta>();

  Signature in_first;
  in_first.set(first.GetComponentBit<Beta>());

  Signature in_second;
  in_second.set(second.GetComponentBit<Beta>());

  EXPECT_NE(in_first, in_second)
      << "Beta is bit 1 in the first world and bit 0 in the second";
}

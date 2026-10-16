// Damage rules on Health: no ECS, no bgfx.

#include "game/health.h"

#include <gtest/gtest.h>

namespace game {

namespace {

TEST(Health, DamageLowersCurrent) {
  Health health{.max = 100, .current = 100};
  EXPECT_EQ(ApplyDamage(health, 25), 75);
  EXPECT_EQ(health.current, 75);
  EXPECT_EQ(health.max, 100);
}

TEST(Health, DamageClampsAtZero) {
  Health health{.max = 100, .current = 10};
  EXPECT_EQ(ApplyDamage(health, 25), 0);
}

TEST(Health, ZeroStaysZero) {
  Health health{.max = 100, .current = 0};
  EXPECT_EQ(ApplyDamage(health, 25), 0);
}

TEST(Health, ReviveRestoresFullHealth) {
  Health health{.max = 80, .current = 0};
  Revive(health);
  EXPECT_EQ(health.current, 80);
  EXPECT_FALSE(IsDead(health));
}

TEST(Health, DeadOnlyAtZero) {
  EXPECT_FALSE(IsDead(Health{.max = 100, .current = 1}));
  EXPECT_TRUE(IsDead(Health{.max = 100, .current = 0}));
}

}  // namespace

}  // namespace game

// Enemy decisions: no ECS, no bgfx.

#include "game/enemy_rules.h"

#include <gtest/gtest.h>

namespace game {

namespace {

TEST(EnemyRules, ActionFollowsDistance) {
  const EnemyRanges ranges{.engage_m = 8.0F, .run_m = 15.0F};
  EXPECT_EQ(ChooseEnemyAction(3.0F, ranges), EnemyAction::kEngage);
  EXPECT_EQ(ChooseEnemyAction(8.0F, ranges), EnemyAction::kEngage);
  EXPECT_EQ(ChooseEnemyAction(10.0F, ranges), EnemyAction::kWalk);
  EXPECT_EQ(ChooseEnemyAction(15.0F, ranges), EnemyAction::kWalk);
  EXPECT_EQ(ChooseEnemyAction(30.0F, ranges), EnemyAction::kRun);
}

TEST(EnemyRules, BlendGoesFromWalkToRun) {
  EXPECT_FLOAT_EQ(LocomotionBlend(1.5F, 1.5F, 3.5F), 0.0F);
  EXPECT_FLOAT_EQ(LocomotionBlend(2.5F, 1.5F, 3.5F), 0.5F);
  EXPECT_FLOAT_EQ(LocomotionBlend(3.5F, 1.5F, 3.5F), 1.0F);
  // Outside the two clips' speeds it holds the nearer one
  EXPECT_FLOAT_EQ(LocomotionBlend(0.5F, 1.5F, 3.5F), 0.0F);
  EXPECT_FLOAT_EQ(LocomotionBlend(9.0F, 1.5F, 3.5F), 1.0F);
}

TEST(EnemyRules, BlendCopesWithEqualSpeeds) {
  EXPECT_FLOAT_EQ(LocomotionBlend(1.0F, 2.0F, 2.0F), 0.0F);
  EXPECT_FLOAT_EQ(LocomotionBlend(2.0F, 2.0F, 2.0F), 1.0F);
}

}  // namespace

}  // namespace game

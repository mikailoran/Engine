#include "game/enemy_system.h"

#include <bx/math.h>

#include <optional>

#include "engine/ecs/components/animator.h"
#include "engine/ecs/components/character_body.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "game/enemy.h"
#include "game/enemy_rules.h"
#include "game/player.h"

using engine::Animator;
using engine::CharacterBody;
using engine::Ecs;
using engine::Entity;
using engine::PlayOptions;
using engine::Transform;

namespace game {

namespace {

/// Closer than this to the player, an enemy keeps its facing.
constexpr float kMinFacingDistance = 0.01F;
/// Crossfade of a shot in and out, in s.
constexpr float kShotFadeInS = 0.05F;
constexpr float kShotFadeOutS = 0.15F;

/**
 * @brief The rotation turning a model that faces +Z (glTF's front) along
 * the horizontal direction @p to.
 */
auto Facing(const bx::Vec3& to) -> bx::Quaternion {
  return bx::fromAxisAngle({0.0F, 1.0F, 0.0F}, bx::atan2(to.x, to.z));
}

/** @brief Stands @p enemy still, weapon held. */
void Stand(const Enemy& enemy, CharacterBody& body, Animator& animator) {
  body.velocity.x = 0.0F;
  body.velocity.z = 0.0F;
  Play(animator, kBaseLayer, enemy.clips.idle);
  Play(animator, kUpperLayer, enemy.clips.hold);
}

/**
 * @brief Moves @p enemy at the player, @p to away, and plays the clips for
 * what it decides; fires on its cooldown when engaging.
 */
void Pursue(Enemy& enemy, const bx::Vec3& to, float dt, Transform& transform,
            CharacterBody& body, Animator& animator) {
  const float distance = bx::length(to);
  const EnemyAction action = ChooseEnemyAction(distance, enemy.ranges);
  float speed = 0.0F;
  if (action == EnemyAction::kRun) {
    speed = enemy.run_speed;
  } else if (action == EnemyAction::kWalk) {
    speed = enemy.walk_speed;
  }

  if (distance > kMinFacingDistance) {
    transform.rotation = Facing(to);
    const bx::Vec3 velocity = bx::mul(to, speed / distance);
    body.velocity.x = velocity.x;
    body.velocity.z = velocity.z;
  }
  if (speed > 0.0F) {
    PlayBlend(animator, kBaseLayer, enemy.clips.walk, enemy.clips.run,
              LocomotionBlend(speed, enemy.walk_speed, enemy.run_speed));
    Play(animator, kUpperLayer, enemy.clips.hold);
    return;
  }

  Play(animator, kBaseLayer, enemy.clips.idle);
  Play(animator, kUpperLayer, enemy.clips.aim);
  enemy.fire_cooldown_s -= dt;
  if (enemy.fire_cooldown_s <= 0.0F) {
    Play(animator, kShotLayer, enemy.clips.fire,
         PlayOptions{.fade_s = kShotFadeInS, .loop = false, .restart = true});
    enemy.fire_cooldown_s = enemy.fire_interval_s;
  }
}

}  // namespace

auto DriveEnemies(Ecs& ecs, float dt, bool has_control) -> void {
  // Enemies chase the player's feet
  std::optional<bx::Vec3> target;
  ecs.View<Player, Transform>().ForEach(
      [&target](Entity /*entity*/, const Player& /*player*/,
                const Transform& transform) -> void {
        target = transform.position;
      });

  ecs.View<Enemy, Transform, CharacterBody, Animator>().ForEach(
      [&](Entity /*entity*/, Enemy& enemy, Transform& transform,
          CharacterBody& body, Animator& animator) -> void {
        if (has_control && target) {
          bx::Vec3 to = bx::sub(*target, transform.position);
          to.y = 0.0F;
          Pursue(enemy, to, dt, transform, body, animator);
        } else {
          Stand(enemy, body, animator);
        }
        // A finished shot fades out, leaving the stance beneath
        if (IsFinished(animator, kShotLayer)) {
          Stop(animator, kShotLayer, kShotFadeOutS);
        }
      });
}

}  // namespace game

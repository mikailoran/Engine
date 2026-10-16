#include "game/enemy_system.h"

#include <bx/math.h>

#include <optional>

#include "engine/ecs/components/animator.h"
#include "engine/ecs/components/attached_meshes.h"
#include "engine/ecs/components/character_body.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/ecs/systems/attachments.h"
#include "engine/engine.h"
#include "game/aim.h"
#include "game/enemy.h"
#include "game/enemy_rules.h"
#include "game/player.h"

using engine::Animator;
using engine::AttachedMeshes;
using engine::AttachmentMatrix;
using engine::CharacterBody;
using engine::Ecs;
using engine::Engine;
using engine::Entity;
using engine::PlayOptions;
using engine::Transform;

namespace game {

namespace {

/// Closer than this to the player, an enemy keeps its facing.
constexpr float kMinFacingDistance = 0.01F;
/// Height above the player's feet enemies aim at, in m.
constexpr float kChestHeight = 1.3F;
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
 * @return Whether a shot started this frame.
 */
auto Pursue(Enemy& enemy, const bx::Vec3& to, float dt, Transform& transform,
            CharacterBody& body, Animator& animator) -> bool {
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
    return false;
  }

  Play(animator, kBaseLayer, enemy.clips.idle);
  Play(animator, kUpperLayer, enemy.clips.aim);
  enemy.fire_cooldown_s -= dt;
  if (enemy.fire_cooldown_s > 0.0F) {
    return false;
  }
  Play(animator, kShotLayer, enemy.clips.fire,
       PlayOptions{.fade_s = kShotFadeInS, .loop = false, .restart = true});
  enemy.fire_cooldown_s = enemy.fire_interval_s;
  return true;
}

/** @brief The player's feet, if there is a player. */
auto PlayerFeet(Ecs& ecs) -> std::optional<bx::Vec3> {
  std::optional<bx::Vec3> feet;
  ecs.View<Player, Transform>().ForEach(
      [&feet](Entity /*entity*/, const Player& /*player*/,
              const Transform& transform) -> void {
        feet = transform.position;
      });
  return feet;
}

}  // namespace

auto DriveEnemies(Ecs& ecs, float dt, bool has_control) -> void {
  const auto target = PlayerFeet(ecs);
  ecs.View<Enemy, Transform, CharacterBody, Animator>().ForEach(
      [&](Entity entity, Enemy& enemy, Transform& transform,
          CharacterBody& body, Animator& animator) -> void {
        bool fired = false;
        if (has_control && target) {
          bx::Vec3 to = bx::sub(*target, transform.position);
          to.y = 0.0F;
          fired = Pursue(enemy, to, dt, transform, body, animator);
        } else {
          Stand(enemy, body, animator);
        }
        // The trigger is pulled for one frame per shot
        if (auto* aim = ecs.TryGetComponent<Aim>(entity)) {
          aim->weapon_trigger = fired;
        }
        // A finished shot fades out, leaving the stance beneath
        if (IsFinished(animator, kShotLayer)) {
          Stop(animator, kShotLayer, kShotFadeOutS);
        }
      });
}

auto AimEnemies(Engine& engine) -> void {
  Ecs& ecs = engine.World();
  const auto target = PlayerFeet(ecs);
  ecs.View<Enemy, Aim, AttachedMeshes>().ForEach(
      [&](Entity entity, const Enemy& enemy, Aim& aim,
          const AttachedMeshes& attached) -> void {
        if (enemy.gun_attachment >= attached.meshes.size()) {
          return;
        }
        const auto gun =
            AttachmentMatrix(ecs, engine.Assets(), entity,
                             attached.meshes.at(enemy.gun_attachment));
        aim.muzzle = bx::mul(enemy.muzzle, gun.data());
        // Straight at the chest, or along the barrel with no player
        const bx::Vec3 chest =
            target ? bx::add(*target, {0.0F, kChestHeight, 0.0F})
                   : bx::mul(bx::add(enemy.muzzle, {0.0F, 0.0F, -1.0F}),
                             gun.data());
        aim.ray = {.pos = aim.muzzle,
                   .dir = bx::normalize(bx::sub(chest, aim.muzzle))};
      });
}

}  // namespace game

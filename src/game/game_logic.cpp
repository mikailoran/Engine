#include "game/game_logic.h"

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <stdexcept>
#include <string>

#include "engine/ecs/components/animator.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/engine.h"
#include "engine/physics/physics_world.h"
#include "engine/resource/animation_handles.h"
#include "engine/resource/asset_registry.h"
#include "engine/scene/json_read.h"
#include "game/aim.h"
#include "game/enemy.h"
#include "game/health.h"
#include "game/player.h"
#include "game/weapon.h"

namespace game {

namespace {

/**
 * @brief Adds a Player; every field is optional and positive. Speeds in m/s,
 * the eye height in m. Its spawn point is where its Transform puts it.
 */
void LoadPlayer(const nlohmann::json& data, engine::Entity entity,
                engine::Ecs& ecs, engine::AssetRegistry& /*assets*/,
                engine::physics::PhysicsWorld& /*physics*/) {
  engine::CheckKeys(
      data, {"walk_speed", "sprint_multiplier", "jump_speed", "eye_height"});
  Player player{};
  engine::ReadPositive(data, "walk_speed", player.walk_speed);
  engine::ReadPositive(data, "sprint_multiplier", player.sprint_multiplier);
  engine::ReadPositive(data, "jump_speed", player.jump_speed);
  engine::ReadPositive(data, "eye_height", player.eye_height);
  // The transform loader runs first, being the engine's
  if (const auto* transform = ecs.TryGetComponent<engine::Transform>(entity)) {
    player.spawn_point = transform->position;
  }
  ecs.AddComponent(entity, player);
}

/**
 * @brief Adds a Health. `max` is optional and positive; `current` defaults to
 * `max` and must be within 0..max, or the load throws.
 */
void LoadHealth(const nlohmann::json& data, engine::Entity entity,
                engine::Ecs& ecs, engine::AssetRegistry& /*assets*/,
                engine::physics::PhysicsWorld& /*physics*/) {
  engine::CheckKeys(data, {"max", "current"});
  Health health{};
  engine::ReadPositive(data, "max", health.max);
  // Starts full unless current is given; 0 is a valid start
  health.current = data.value("current", health.max);
  if (health.current < 0 || health.current > health.max) {
    throw std::runtime_error("health: current must be within 0..max");
  }
  ecs.AddComponent(entity, health);
}

/**
 * @brief Adds a Weapon and the Aim it fires along. Every field is optional
 * and positive: damage in health points, cooldown_s and beam_s in s, range_m
 * in m.
 */
void LoadWeapon(const nlohmann::json& data, engine::Entity entity,
                engine::Ecs& ecs, engine::AssetRegistry& /*assets*/,
                engine::physics::PhysicsWorld& /*physics*/) {
  engine::CheckKeys(data, {"damage", "cooldown_s", "range_m", "beam_s"});
  Weapon weapon{};
  engine::ReadPositive(data, "damage", weapon.damage);
  engine::ReadPositive(data, "cooldown_s", weapon.cooldown_s);
  engine::ReadPositive(data, "range_m", weapon.range_m);
  engine::ReadPositive(data, "beam_s", weapon.beam_s);
  ecs.AddComponent(entity, weapon);
  // Runtime: whoever controls the entity writes it every frame
  ecs.AddComponent(entity, Aim{});
}

/** @brief The clip named by @p data's @p key in @p set; throws if absent. */
auto FindClip(const nlohmann::json& data, const char* key,
              const engine::AssetRegistry& assets, engine::ClipSetHandle set)
    -> engine::ClipHandle {
  const auto name = data.at(key).get<std::string>();
  const engine::ClipHandle clip = assets.FindClip(set, name);
  if (!clip.IsValid()) {
    throw std::runtime_error("enemy: no clip named '" + name + "'");
  }
  return clip;
}

/**
 * @brief Adds an Enemy. Needs the entity's Animator; "clips" names the
 * idle, walk, run, hold, aim and fire clips in it. Speeds (m/s), ranges (m),
 * "fire_interval_s", "upper_body_joint" (the upper layers' mask root,
 * default "spine_01"), "gun_attachment" (an index into its attachments) and
 * "muzzle" (a point in the gun's frame) are optional.
 */
void LoadEnemy(const nlohmann::json& data, engine::Entity entity,
               engine::Ecs& ecs, engine::AssetRegistry& assets,
               engine::physics::PhysicsWorld& /*physics*/) {
  engine::CheckKeys(
      data, {"walk_speed", "run_speed", "engage_m", "run_m", "fire_interval_s",
             "upper_body_joint", "gun_attachment", "muzzle", "clips"});
  // The engine's animator loader runs first, being registered first
  auto* animator = ecs.TryGetComponent<engine::Animator>(entity);
  if (animator == nullptr) {
    throw std::runtime_error("enemy needs an animator");
  }

  Enemy enemy{};
  engine::ReadPositive(data, "walk_speed", enemy.walk_speed);
  engine::ReadPositive(data, "run_speed", enemy.run_speed);
  engine::ReadPositive(data, "engage_m", enemy.ranges.engage_m);
  engine::ReadPositive(data, "run_m", enemy.ranges.run_m);
  engine::ReadPositive(data, "fire_interval_s", enemy.fire_interval_s);
  enemy.gun_attachment = data.value("gun_attachment", enemy.gun_attachment);
  if (data.contains("muzzle")) {
    enemy.muzzle = engine::ReadVec3(data.at("muzzle"));
  }

  const auto& clips = data.at("clips");
  engine::CheckKeys(clips, {"idle", "walk", "run", "hold", "aim", "fire"});
  enemy.clips = {.idle = FindClip(clips, "idle", assets, animator->clips),
                 .walk = FindClip(clips, "walk", assets, animator->clips),
                 .run = FindClip(clips, "run", assets, animator->clips),
                 .hold = FindClip(clips, "hold", assets, animator->clips),
                 .aim = FindClip(clips, "aim", assets, animator->clips),
                 .fire = FindClip(clips, "fire", assets, animator->clips)};

  // Stance and shots play over the upper body only
  const auto upper = data.value("upper_body_joint", std::string("spine_01"));
  SetLayer(*animator, kUpperLayer, {.weight = 1.0F, .mask = upper});
  SetLayer(*animator, kShotLayer, {.weight = 1.0F, .mask = upper});
  ecs.AddComponent(entity, enemy);
}

}  // namespace

void RegisterGameLogic(engine::Engine& engine) {
  engine.World().RegisterComponent<Player>();
  engine.World().RegisterComponent<Health>();
  engine.World().RegisterComponent<Weapon>();
  engine.World().RegisterComponent<Aim>();
  engine.World().RegisterComponent<Enemy>();
  engine.RegisterSceneLoader("player", LoadPlayer);
  engine.RegisterSceneLoader("health", LoadHealth);
  engine.RegisterSceneLoader("weapon", LoadWeapon);
  engine.RegisterSceneLoader("enemy", LoadEnemy);
}

}  // namespace game

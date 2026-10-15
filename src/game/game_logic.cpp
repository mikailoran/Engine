#include "game/game_logic.h"

#include <nlohmann/json.hpp>
#include <stdexcept>

#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/engine.h"
#include "engine/physics/physics_world.h"
#include "engine/resource/asset_registry.h"
#include "engine/scene/json_read.h"
#include "game/aim.h"
#include "game/health.h"
#include "game/player.h"
#include "game/weapon.h"

namespace game {

namespace {

/**
 * @brief Adds a Player; every field is optional and positive. Speeds in m/s,
 * the eye height in m.
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

}  // namespace

void RegisterGameLogic(engine::Engine& engine) {
  engine.World().RegisterComponent<Player>();
  engine.World().RegisterComponent<Health>();
  engine.World().RegisterComponent<Weapon>();
  engine.World().RegisterComponent<Aim>();
  engine.RegisterSceneLoader("player", LoadPlayer);
  engine.RegisterSceneLoader("health", LoadHealth);
  engine.RegisterSceneLoader("weapon", LoadWeapon);
}

}  // namespace game

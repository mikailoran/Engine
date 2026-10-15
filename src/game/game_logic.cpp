#include "game/game_logic.h"

#include <nlohmann/json_fwd.hpp>

#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/engine.h"
#include "engine/physics/physics_world.h"
#include "engine/resource/asset_registry.h"
#include "engine/scene/json_read.h"
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

void LoadHealth(const nlohmann::json& data, engine::Entity entity,
                engine::Ecs& ecs, engine::AssetRegistry& /*assets*/,
                engine::physics::PhysicsWorld& /*physics*/) {
  engine::CheckKeys(data, {"max", "current"});
  Health health{};
  engine::ReadPositive(data, "max", health.max);
  engine::ReadPositive(data, "current", health.current);
  ecs.AddComponent(entity, health);
}

void LoadWeapon(const nlohmann::json& data, engine::Entity entity,
                engine::Ecs& ecs, engine::AssetRegistry& /*assets*/,
                engine::physics::PhysicsWorld& /*physics*/) {
  engine::CheckKeys(data, {"damage", "cooldown"});
  Weapon weapon{};
  engine::ReadPositive(data, "damage", weapon.damage);
  engine::ReadPositive(data, "cooldown", weapon.cooldown_s);
  ecs.AddComponent(entity, weapon);
}

}  // namespace

void RegisterGameLogic(engine::Engine& engine) {
  engine.World().RegisterComponent<Player>();
  engine.World().RegisterComponent<Health>();
  engine.World().RegisterComponent<Weapon>();
  engine.RegisterSceneLoader("player", LoadPlayer);
  engine.RegisterSceneLoader("health", LoadHealth);
  engine.RegisterSceneLoader("weapon", LoadWeapon);
}

}  // namespace game

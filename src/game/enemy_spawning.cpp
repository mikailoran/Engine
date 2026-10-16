#include "game/enemy_spawning.h"

#include <bx/math.h>

#include <vector>

#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/types.h"
#include "engine/engine.h"
#include "game/aim.h"
#include "game/player.h"

using engine::Engine;
using engine::Entity;
using engine::Transform;

namespace game {

namespace {

// Prefab each spawn instantiates
constexpr const char* kEnemyPrefab = "assets/prefabs/enemy.json";
// How far a spawn ray reaches, in m
constexpr float kSpawnRangeM = 100.0F;

}  // namespace

// TODO: move to the devtools console (ADR 0013)
auto HandleEnemySpawns(Engine& engine) -> void {
  // Find spawn points first: spawning inside ForEach may add viewed components
  std::vector<bx::Vec3> spawn_points;
  engine.World().View<Player, Aim>().ForEach(
      [&](Entity entity, const Player& /*player*/, const Aim& aim) -> void {
        if (!aim.spawn_trigger) {
          return;
        }
        if (const auto hit = engine.CastRay(aim.ray, kSpawnRangeM, entity)) {
          spawn_points.push_back(hit->point);
        }
      });

  for (const auto& point : spawn_points) {
    engine.SpawnPrefab(kEnemyPrefab, Transform{.position = point});
  }
}

}  // namespace game

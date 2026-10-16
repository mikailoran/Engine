#include "game/shooting_system.h"

#include <bx/bounds.h>
#include <bx/bx.h>
#include <bx/math.h>

#include <algorithm>
#include <format>
#include <iostream>
#include <string>
#include <vector>

#include "engine/ecs/components/character_body.h"
#include "engine/ecs/components/lifetime.h"
#include "engine/ecs/components/name.h"
#include "engine/ecs/components/ragdoll.h"
#include "engine/ecs/components/renderable.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/engine.h"
#include "game/aim.h"
#include "game/enemy.h"
#include "game/health.h"
#include "game/weapon.h"

using engine::CharacterBody;
using engine::Ecs;
using engine::Engine;
using engine::Entity;
using engine::Lifetime;
using engine::Name;
using engine::Ragdoll;
using engine::Renderable;
using engine::Transform;

namespace game {

namespace {

/// How long a ragdolled body lies before it's removed, in s.
constexpr float kCorpseSeconds = 15.0F;

/** @brief One shot fired this frame, kept until its beam is spawned. */
struct Shot {
  /// Muzzle point the beam starts at; the player's ray starts at the eyes.
  bx::Vec3 from{bx::InitZero};
  /// Hit point, or full range on a miss.
  bx::Vec3 to{bx::InitZero};
  /// How long the beam shows, in s.
  float beam_s{0.0F};
};

/**
 * @brief Rotation turning a mesh's +Y axis along the segment @p from → @p to.
 *
 * Only for meshes symmetric end to end (a cylinder): it may point +Y at
 * @p from instead of @p to. Identity for a zero-length segment.
 */
auto AlongSegment(const bx::Vec3& from, const bx::Vec3& to) -> bx::Quaternion {
  const bx::Vec3 delta = bx::sub(to, from);
  const float length = bx::length(delta);
  if (length < 1e-6F) {
    return bx::InitIdentity;
  }
  // Flip into the upper half: a cylinder looks the same either way round
  const float sign = delta.y < 0.0F ? -1.0F : 1.0F;
  const bx::Vec3 dir = bx::mul(delta, sign / length);
  // Shortest arc from +Y: {cross(Y, dir), 1 + dot(Y, dir)}, normalized
  const float inv_norm = 1.0F / bx::sqrt(2.0F * (1.0F + dir.y));
  return {dir.z * inv_norm, 0.0F, -dir.x * inv_norm, (1.0F + dir.y) * inv_norm};
}

/** @brief Removes @p entity's Component if it has one. */
template <class Component>
void RemoveIfPresent(Ecs& ecs, Entity entity) {
  if (ecs.HasComponent<Component>(entity)) {
    ecs.RemoveComponent<Component>(entity);
  }
}

/**
 * @brief Kills @p entity: a character with a ragdoll goes limp and stays for
 * kCorpseSeconds, anything else is destroyed.
 * @pre Not inside a ForEach: it removes components.
 */
void Die(Ecs& ecs, Entity entity) {
  auto* ragdoll = ecs.TryGetComponent<Ragdoll>(entity);
  if (ragdoll == nullptr) {
    ecs.DestroyEntity(entity);
    return;
  }
  ragdoll->simulate = true;
  // A body no longer walks, chases, shoots or takes damage
  RemoveIfPresent<CharacterBody>(ecs, entity);
  RemoveIfPresent<Enemy>(ecs, entity);
  RemoveIfPresent<Weapon>(ecs, entity);
  RemoveIfPresent<Aim>(ecs, entity);
  RemoveIfPresent<Health>(ecs, entity);
  ecs.AddComponent(entity, Lifetime{kCorpseSeconds});
}

}  // namespace

auto HandleShots(Engine& engine) -> void {
  std::vector<Shot> shots;
  // Dying removes viewed components, so it waits for the loop to end
  std::vector<Entity> deaths;
  engine.World().View<Aim, Weapon>().ForEach(
      [&](Entity entity, const Aim& aim, Weapon& weapon) -> void {
        // Fire only on a trigger pull with the weapon ready
        if (!aim.weapon_trigger || weapon.cooldown_left_s > 0.0F) {
          return;
        }
        Shot shot{.from = aim.muzzle, .beam_s = weapon.beam_s};
        // Start the cooldown
        weapon.cooldown_left_s = weapon.cooldown_s;

        const auto hit = engine.CastRay(aim.ray, weapon.range_m, entity);
        if (!hit) {
          shot.to = bx::add(shot.from, bx::mul(aim.ray.dir, weapon.range_m));
          shots.push_back(shot);
          return;
        }
        shot.to = hit->point;
        shots.push_back(shot);

        // Damage the hit entity
        auto& ecs = engine.World();
        if (auto* health = ecs.TryGetComponent<Health>(hit->entity)) {
          const auto remaining_health = ApplyDamage(*health, weapon.damage);
          // Enemies shoot too, and may hit each other
          const auto name_of = [&ecs](Entity named) -> std::string {
            const auto* name = ecs.TryGetComponent<Name>(named);
            return name != nullptr ? name->value : "<unnamed>";
          };
          std::cout << std::format(
                           "{} shot {} for {} damage. Remaining "
                           "health: {}",
                           name_of(entity), name_of(hit->entity), weapon.damage,
                           remaining_health)
                    << "\n";

          if (IsDead(*health)) {
            deaths.push_back(hit->entity);
          }
        }
      });

  auto& ecs = engine.World();
  for (const Entity entity : deaths) {
    Die(ecs, entity);
  }

  // Make the projectile beam
  auto beam_mesh_handle = engine.Assets().LoadMesh("assets/meshes/beam.glb");
  for (const auto& shot : shots) {
    auto beam = ecs.CreateEntity();
    ecs.AddComponent(beam, Renderable{
                               .mesh_handle = beam_mesh_handle,
                           });
    // .color = {1.0F, 0.85F, 0.3F, 1.0F}});
    ecs.AddComponent(beam, Lifetime{shot.beam_s});
    const auto middle_pos = bx::div(bx::add(shot.from, shot.to), 2.0F);
    const float length = bx::length(bx::sub(shot.to, shot.from));
    ecs.AddComponent(beam,
                     Transform{.position = middle_pos,
                               .rotation = AlongSegment(shot.from, shot.to),
                               .scale = {0.05F, length, 0.05F}});
  }
}

auto UpdateCooldowns(Ecs& ecs, float dt) -> void {
  ecs.View<Weapon>().ForEach([dt](Entity /*entity*/, Weapon& weapon) -> void {
    weapon.cooldown_left_s = std::max(0.0F, weapon.cooldown_left_s - dt);
  });
}

}  // namespace game

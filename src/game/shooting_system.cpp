#include "game/shooting_system.h"

#include <bx/bounds.h>
#include <bx/bx.h>
#include <bx/math.h>

#include <algorithm>
#include <format>
#include <iostream>
#include <vector>

#include "engine/ecs/components/lifetime.h"
#include "engine/ecs/components/name.h"
#include "engine/ecs/components/renderable.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/engine.h"
#include "game/aim.h"
#include "game/health.h"
#include "game/weapon.h"

using engine::Ecs;
using engine::Engine;
using engine::Entity;
using engine::Lifetime;
using engine::Name;
using engine::Renderable;
using engine::Transform;

namespace game {

namespace {

/** @brief One shot fired this frame, kept until its beam is spawned. */
struct Shot {
  /// Muzzle point the beam starts at; the ray itself starts at the eyes.
  bx::Vec3 from{bx::InitZero};
  /// Hit point, or full range on a miss.
  bx::Vec3 to{bx::InitZero};
  /// How long the beam shows, in s.
  float beam_s{0.0F};
};

/// Muzzle offset from the eyes in m, right and down in the view.
constexpr float kMuzzleRight = 0.2F;
constexpr float kMuzzleDown = 0.15F;

/**
 * @brief Where a beam fired along @p ray starts: below and right of its origin.
 * The ray must not be vertical (look pitch stays under 90°).
 */
auto Muzzle(const bx::Ray& ray) -> bx::Vec3 {
  const bx::Vec3 right = bx::normalize(bx::cross(ray.dir, {0.0F, 1.0F, 0.0F}));
  const bx::Vec3 up = bx::cross(right, ray.dir);
  return bx::add(
      ray.pos, bx::sub(bx::mul(right, kMuzzleRight), bx::mul(up, kMuzzleDown)));
}

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

}  // namespace

auto HandleShots(Engine& engine) -> void {
  std::vector<Shot> shots;
  engine.World().View<Aim, Weapon>().ForEach(
      [&](Entity entity, const Aim& aim, Weapon& weapon) -> void {
        // Fire only on a trigger pull with the weapon ready
        if (!aim.weapon_trigger || weapon.cooldown_left_s > 0.0F) {
          return;
        }
        Shot shot{.from = Muzzle(aim.ray), .beam_s = weapon.beam_s};
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
          const auto* name = ecs.TryGetComponent<Name>(hit->entity);
          std::cout << std::format(
                           "Player shot entity {} for {} damage. Remaining "
                           "health: {}",
                           name ? name->value : "<unnamed>", weapon.damage,
                           remaining_health)
                    << "\n";

          // TODO: Use a death system
          if (remaining_health <= 0) {
            ecs.DestroyEntity(hit->entity);
          }
        }
      });

  // Make the projectile beam
  auto& ecs = engine.World();
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

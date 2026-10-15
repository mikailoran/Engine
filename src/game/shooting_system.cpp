#include "game/shooting_system.h"

#include <bx/math.h>

#include <format>
#include <iostream>

#include "engine/ecs/components/camera.h"
#include "engine/ecs/components/lifetime.h"
#include "engine/ecs/components/name.h"
#include "engine/ecs/components/renderable.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/engine.h"
#include "engine/math/rotation.h"
#include "engine/platform/frame_context.h"
#include "game/aim.h"
#include "game/health.h"
#include "game/player.h"
#include "game/weapon.h"

using engine::Camera;
using engine::Ecs;
using engine::Engine;
using engine::Entity;
using engine::FrameContext;
using engine::Lifetime;
using engine::Name;
using engine::Renderable;
using engine::Transform;
using engine::YawPitch;

namespace {
struct Shot {
  bx::Vec3 from{bx::InitZero};
  bx::Vec3 to{bx::InitZero};
  float distance{0.0F};
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

}  // namespace

namespace game {

// auto Shoot(engine::Engine& engine) -> void {
//   bool shot = TryHit(engine, angles);
//   if (!shot) {
//     return;
//   }
//   // Make the projectile beam
//   auto& ecs = engine.World();
//   auto beam = ecs.CreateEntity();
//   auto beam_mesh_handle =
//       engine.Assets().LoadMesh("assets/meshes/cylinder.glb");
//   ecs.AddComponent(beam, Lifetime{0.5F});
//   ecs.AddComponent(beam, Transform{.position = eye, .rotation = look});
//   ecs.AddComponent(beam, Renderable{.mesh_handle = beam_mesh_handle});
// }

auto HandleShots(Engine& engine) -> void {
  std::vector<Shot> shots;
  engine.World().View<Aim, Weapon>().ForEach(
      [&](Entity entity, Aim& aim, Weapon& weapon) -> void {
        // Weapon still on cooldown.
        if (!aim.trigger || weapon.cooldown_left_s > 0.0F) {
          return;
        }
        Shot shot{.from = aim.ray.pos};
        // Increment weapon cooldown
        weapon.cooldown_left_s = weapon.cooldown_s;
        aim.trigger = false;

        const auto hit = engine.CastRay(aim.ray, weapon.range_m, entity);
        if (!hit) {
          shot.to = bx::add(shot.from, bx::mul(aim.ray.dir, weapon.range_m));
          shot.distance = weapon.range_m;
          shots.push_back(shot);
          return;
        }
        shot.to = hit->point;
        shot.distance = hit->distance;
        shots.push_back(shot);

        // Damage the hit entity
        auto& ecs = engine.World();
        if (auto* health = ecs.TryGetComponent<Health>(hit->entity)) {
          health->current = std::max(0, health->current - weapon.damage);
          const auto* name = ecs.TryGetComponent<Name>(hit->entity);
          std::cout << std::format(
                           "Player shot entity {} for {} damage. Remaining "
                           "health: {}",
                           name ? name->value : "<unnamed>", weapon.damage,
                           health->current)
                    << "\n";
        }
      });

  // Make the projectile beam
  auto& ecs = engine.World();
  auto beam_mesh_handle =
      engine.Assets().LoadMesh("assets/meshes/cylinder.glb");
  for (const auto& shot : shots) {
    auto beam = ecs.CreateEntity();
    ecs.AddComponent(beam, Renderable{.mesh_handle = beam_mesh_handle});
    ecs.AddComponent(beam, Lifetime{0.2F});
    const auto middle_pos = bx::div(bx::add(shot.from, shot.to), 2.0F);
    ecs.AddComponent(beam,
                     Transform{.position = middle_pos,
                               .rotation = AlongSegment(shot.from, shot.to),
                               .scale = {0.05F, shot.distance, 0.05F}});
  }
}

auto UpdateCooldowns(Ecs& ecs, float dt) -> void {
  ecs.View<Weapon>().ForEach([dt](Entity /*entity*/, Weapon& weapon) -> void {
    weapon.cooldown_left_s = std::max(0.0F, weapon.cooldown_left_s - dt);
  });
}

}  // namespace game
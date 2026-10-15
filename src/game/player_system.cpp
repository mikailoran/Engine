#include "game/player_system.h"

#include <bx/math.h>

#include <algorithm>
#include <format>
#include <iostream>

#include "engine/ecs/components/character_body.h"
#include "engine/ecs/components/character_link.h"
#include "engine/ecs/components/lifetime.h"
#include "engine/ecs/components/name.h"
#include "engine/ecs/components/renderable.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/engine.h"
#include "engine/math/rotation.h"
#include "engine/platform/frame_context.h"
#include "engine/platform/input.h"
#include "game/aim.h"
#include "game/health.h"
#include "game/player.h"
#include "game/weapon.h"

using engine::CharacterBody;
using engine::CharacterLink;
using engine::Ecs;
using engine::Engine;
using engine::Entity;
using engine::FrameContext;
using engine::Input;
using engine::Key;
using engine::Lifetime;
using engine::Name;
using engine::Renderable;
using engine::Transform;
using engine::YawPitchToQuat;

namespace game {

namespace {

/**
 * @brief The level direction W/A/S/D ask for, turned by @p facing.
 * @return A unit vector, or zero when no key is held.
 */
auto WalkDirection(const Input& input, const bx::Quaternion& facing)
    -> bx::Vec3 {
  // Local -Z is forward, +X is right
  bx::Vec3 local{0.0F};
  if (input.Down(Key::kW)) {
    local.z -= 1.0F;
  }
  if (input.Down(Key::kS)) {
    local.z += 1.0F;
  }
  if (input.Down(Key::kD)) {
    local.x += 1.0F;
  }
  if (input.Down(Key::kA)) {
    local.x -= 1.0F;
  }
  if (local.x == 0.0F && local.z == 0.0F) {
    return local;
  }
  return bx::mul(bx::normalize(local), facing);
}

}  // namespace

void PlayerSystem::SetViewCamera(Entity camera) { camera_ = camera; }

void PlayerSystem::Control(Ecs& ecs, const FrameContext& ctx,
                           bool has_control) {
  const Input& input = ctx.input;
  // Accumulate mouse input into stored angles_
  if (has_control) {
    // Moving the mouse right turns clockwise seen from above: negative yaw
    angles_.yaw -= kLookRadiansPerPixel * input.MouseMotion().x;
    angles_.pitch = std::clamp(
        angles_.pitch - (kLookRadiansPerPixel * input.MouseMotion().y),
        -kMaxPitch, kMaxPitch);

    const auto left_click_pressed = input.Pressed(engine::MouseButton::kLeft);
    // Update the player's Aim with the mouse input
    ecs.View<Player, Transform, Aim>().ForEach(
        [&](Entity /*entity*/, const Player& player, const Transform& transform,
            Aim& aim) -> void {
          aim.ray.pos =
              bx::add(transform.position, {0.0F, player.eye_height, 0.0F});
          // Rotate foward direction vector (-z) by look quaternion
          const bx::Quaternion look = YawPitchToQuat(angles_);
          aim.ray.dir = bx::mul(bx::Vec3{0.0F, 0.0F, -1.0F}, look);
          aim.trigger = left_click_pressed;
        });
  }

  // Update player rotation and velocity from keyboard input
  ecs.View<Player, CharacterBody, Transform>().ForEach(
      [&](Entity entity, const Player& player, CharacterBody& body,
          Transform& transform) -> void {
        // The body turns about the vertical only; looking up tilts the camera
        // TODO: Where should Player's transform rotation be updated ideally??
        const bx::Quaternion facing = YawPitchToQuat({.yaw = angles_.yaw});
        transform.rotation = facing;

        bx::Vec3 walk{0.0F};
        if (has_control) {
          const bool sprint =
              input.Down(Key::kLeftShift) || input.Down(Key::kRightShift);
          const auto speed =
              player.walk_speed * (sprint ? player.sprint_multiplier : 1.0F);
          walk = bx::mul(WalkDirection(input, facing), speed);
        }
        // Walking sets the level part; gravity keeps the vertical one
        body.velocity.x = walk.x;
        body.velocity.z = walk.z;

        const auto* link = ecs.TryGetComponent<CharacterLink>(entity);
        if (has_control && input.Pressed(Key::kSpace) && link != nullptr &&
            link->OnGround()) {
          body.velocity.y = player.jump_speed;
        }
      });
}

// auto PlayerSystem::Shoot(Engine& engine) -> void {
//   engine.World().View<Player, Weapon, Transform>().ForEach([&engine,
//                                                             &angles =
//                                                             angles_](
//                                                                Entity entity,
//                                                                const Player&
//                                                                    player,
//                                                                Weapon&
//                                                                weapon, const
//                                                                Transform&
//                                                                    transform)
//                                                                -> void {
//     // Weapon still on cooldown.
//     if (weapon.cooldown_left_s > 0.0F) {
//       return;
//     }
//     // Increment weapon cooldown
//     weapon.cooldown_left_s = weapon.cooldown_s;

//     // Player system angles instead of Transform to get same frame value.
//     const bx::Quaternion look = YawPitchToQuat(angles);
//     const auto eye =
//         bx::add(transform.position, {0.0F, player.eye_height, 0.0F});
//     // Rotate foward direction vector (-z) by look quaternion
//     const auto dir = bx::mul(bx::Vec3{0.0F, 0.0F, -1.0F}, look);

//     // Make the projectile beam
//     auto& ecs = engine.World();
//     auto beam = ecs.CreateEntity();
//     auto beam_mesh_handle =
//         engine.Assets().LoadMesh("assets/meshes/cylinder.glb");
//     ecs.AddComponent(beam, Lifetime{0.5F});
//     ecs.AddComponent(beam, Transform{.position = eye, .rotation = look});
//     ecs.AddComponent(beam, Renderable{.mesh_handle = beam_mesh_handle});

//     const auto hit =
//         engine.CastRay({.pos = eye, .dir = dir}, weapon.range_m, entity);
//     if (!hit) {
//       return;
//     }

//     // Damage the hit entity
//     if (auto* health = ecs.TryGetComponent<Health>(hit->entity)) {
//       health->current = std::max(0, health->current - weapon.damage);
//       const auto* name = ecs.TryGetComponent<Name>(hit->entity);
//       std::cout
//           << std::format(
//                  "Player shot entity {} for {} damage. Remaining health: {}",
//                  name ? name->value : "<unnamed>", weapon.damage,
//                  health->current)
//           << std::endl;
//     }
//   });
// }

void PlayerSystem::PlaceCamera(Ecs& ecs) const {
  if (!camera_) {
    return;
  }
  auto& camera = ecs.GetComponent<Transform>(*camera_);
  ecs.View<Player, Transform>().ForEach(
      [&](Entity /*entity*/, const Player& player,
          const Transform& transform) -> void {
        camera.position =
            bx::add(transform.position, {0.0F, player.eye_height, 0.0F});
        camera.rotation = YawPitchToQuat(angles_);
      });
}

}  // namespace game

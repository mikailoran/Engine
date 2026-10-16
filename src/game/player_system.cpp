#include "game/player_system.h"

#include <bx/math.h>

#include <algorithm>

#include "engine/ecs/components/character_body.h"
#include "engine/ecs/components/character_link.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/math/rotation.h"
#include "engine/platform/frame_context.h"
#include "engine/platform/input.h"
#include "game/aim.h"
#include "game/player.h"

using engine::CharacterBody;
using engine::CharacterLink;
using engine::Ecs;
using engine::Entity;
using engine::FrameContext;
using engine::Input;
using engine::Key;
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
  }

  // Fire only in play mode; PlaceCamera aims once physics has moved us
  const bool weapon_trigger =
      has_control && input.Down(engine::MouseButton::kLeft);
  const bool spawn_trigger =
      has_control && input.Pressed(engine::MouseButton::kRight);
  ecs.View<Player, Aim>().ForEach(
      [weapon_trigger, spawn_trigger](
          Entity /*entity*/, const Player& /*player*/, Aim& aim) -> void {
        aim.weapon_trigger = weapon_trigger;
        aim.spawn_trigger = spawn_trigger;
      });

  // The body turns about the vertical only; looking up tilts the camera
  // TODO: Where should Player's transform rotation be updated ideally??
  const bx::Quaternion facing = YawPitchToQuat({.yaw = angles_.yaw});

  // Update player rotation and velocity from keyboard input
  ecs.View<Player, CharacterBody, Transform>().ForEach(
      [&](Entity entity, const Player& player, CharacterBody& body,
          Transform& transform) -> void {
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

void PlayerSystem::PlaceCamera(Ecs& ecs) const {
  const bx::Quaternion look = YawPitchToQuat(angles_);
  ecs.View<Player, Transform>().ForEach(
      [&](Entity entity, const Player& player,
          const Transform& transform) -> void {
        const bx::Vec3 eyes =
            bx::add(transform.position, {0.0F, player.eye_height, 0.0F});
        // TODO: a camera entity per Player instead of one in the player system?
        if (camera_) {
          auto& camera = ecs.GetComponent<Transform>(*camera_);
          camera.position = eyes;
          camera.rotation = look;
        }
        // Aim from where the camera now draws
        if (auto* aim = ecs.TryGetComponent<Aim>(entity)) {
          aim->ray = {.pos = eyes,
                      .dir = bx::mul(bx::Vec3{0.0F, 0.0F, -1.0F}, look)};
        }
      });
}

}  // namespace game

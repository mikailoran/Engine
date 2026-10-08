#include "devtools/fly_camera_system.h"

#include <bx/math.h>

#include <algorithm>

#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/math/rotation.h"
#include "engine/platform/frame_context.h"
#include "engine/platform/input.h"
#include "engine/platform/screen.h"

using engine::Ecs;
using engine::Entity;
using engine::FrameContext;
using engine::Input;
using engine::Key;
using engine::MouseButton;
using engine::ScreenPosition;
using engine::Transform;
using engine::YawPitchToQuat;

namespace devtools {

namespace {

// Matches the examples' camera this replaces.
constexpr float kMoveSpeed = 30.0F;  // world units per second
constexpr float kTurnRadiansPerPixel = 0.002F;
// Just short of straight up or down, where the view would flip over
constexpr float kMaxPitch = 1.55F;

}  // namespace

void FlyCameraSystem::Update(Ecs& ecs, const FrameContext& ctx,
                             bool ui_has_mouse, bool ui_has_keyboard) {
  const Input& input = ctx.input;

  // Turn by how far the cursor moved since last frame, while right is held
  const ScreenPosition mouse = input.Mouse();
  if (!ui_has_mouse && input.Down(MouseButton::kRight)) {
    // Dragging right turns clockwise seen from above: negative yaw
    angles_.yaw -= kTurnRadiansPerPixel * (mouse.x - last_mouse_.x);
    angles_.pitch = std::clamp(
        angles_.pitch - (kTurnRadiansPerPixel * (mouse.y - last_mouse_.y)),
        -kMaxPitch, kMaxPitch);
  }
  last_mouse_ = mouse;

  if (!camera_) {
    return;
  }

  auto& transform = ecs.GetComponent<Transform>(*camera_);
  transform.rotation = YawPitchToQuat(angles_);

  // Move along the camera's own axes
  const bx::Vec3 right =
      bx::mul(bx::Vec3{1.0F, 0.0F, 0.0F}, transform.rotation);
  const bx::Vec3 up = bx::mul(bx::Vec3{0.0F, 1.0F, 0.0F}, transform.rotation);
  const bx::Vec3 forward =
      bx::mul(bx::Vec3{0.0F, 0.0F, -1.0F}, transform.rotation);

  // Scrolling the UI must not move the camera
  bx::Vec3 move = bx::mul(forward, ui_has_mouse ? 0.0F : input.Wheel());

  // Nor must typing into it
  if (!ui_has_keyboard) {
    if (input.Down(Key::kW)) {
      move = bx::add(move, forward);
    }
    if (input.Down(Key::kS)) {
      move = bx::sub(move, forward);
    }
    if (input.Down(Key::kD)) {
      move = bx::add(move, right);
    }
    if (input.Down(Key::kA)) {
      move = bx::sub(move, right);
    }
    if (input.Down(Key::kE)) {
      move = bx::add(move, up);
    }
    if (input.Down(Key::kQ)) {
      move = bx::sub(move, up);
    }
  }
  transform.position = bx::mad(move, kMoveSpeed * ctx.dt, transform.position);
}

void FlyCameraSystem::SetControlledCamera(Entity camera) { camera_ = camera; }

}  // namespace devtools

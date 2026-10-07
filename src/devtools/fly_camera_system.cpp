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

namespace {

// Matches the examples' camera this replaces.
constexpr float kMoveSpeed = 30.0F;  // world units per second
constexpr float kTurnRadiansPerPixel = 0.002F;
// Just short of straight up or down, where the view would flip over
constexpr float kMaxPitch = 1.55F;

}  // namespace

void FlyCameraSystem::Update(Ecs& ecs, const FrameContext& ctx) {
  const InputState& input = ctx.input;

  // Turn by how far the cursor moved since last frame, while right is held
  const ScreenPosition mouse = input.Mouse();
  if (input.Down(MouseButton::kRight)) {
    angles_.yaw += kTurnRadiansPerPixel * (mouse.x - last_mouse_.x);
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
      bx::mul(bx::Vec3{0.0F, 0.0F, 1.0F}, transform.rotation);

  bx::Vec3 move = bx::mul(forward, input.Wheel());
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
  transform.position = bx::mad(move, kMoveSpeed * ctx.dt, transform.position);
}

void FlyCameraSystem::SetControlledCamera(Entity camera) { camera_ = camera; }

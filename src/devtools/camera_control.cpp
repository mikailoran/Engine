#include "devtools/camera_control.h"

#include <bx/math.h>
#include <camera.h>
#include <entry/entry.h>

#include <algorithm>
#include <cmath>

#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/math/rotation.h"
#include "engine/platform/frame_context.h"

namespace {

// Starting eye position
constexpr bx::Vec3 kStartPosition{0.0F, 1.0F, -5.0F};

constexpr float kStartVerticalAngle = 0.0F;

}  // namespace

CameraControl::CameraControl() {
  cameraCreate();
  cameraSetPosition(kStartPosition);
  cameraSetVerticalAngle(kStartVerticalAngle);
}

void CameraControl::Update(Ecs& ecs, const FrameContext& ctx,
                           const entry::MouseState& mouse) {
  if (!camera_) {
    return;
  }

  cameraUpdate(ctx.dt, mouse);

  // Turn the example camera's eye and target into the Transform's pose
  const bx::Vec3 eye = cameraGetPosition();
  const bx::Vec3 dir = bx::normalize(bx::sub(cameraGetAt(), eye));
  auto& transform = ecs.GetComponent<Transform>(*camera_);
  transform.position = eye;
  transform.rotation =
      YawPitchToQuat({.yaw = std::atan2(dir.x, dir.z),
                      .pitch = std::asin(std::clamp(dir.y, -1.0F, 1.0F))});
}

CameraControl::~CameraControl() { cameraDestroy(); }

void CameraControl::SetCamera(Entity camera) { camera_ = camera; }

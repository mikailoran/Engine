#include "ecs/systems/camera_control.h"

#include <bx/math.h>
#include <camera.h>

#include "ecs/components/camera.h"
#include "ecs/components/transform.h"
#include "ecs/core/ecs.h"
#include "ecs/core/frame_context.h"
#include "ecs/core/types.h"

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

void CameraControl::Update(Ecs& ecs, const FrameContext& ctx) {
  if (ctx.mouse == nullptr || !camera_) {
    return;
  }

  cameraUpdate(ctx.dt, *ctx.mouse);

  // Mirror the resulting pose into the components.
  ecs.GetComponent<Transform>(*camera_).position = cameraGetPosition();
  ecs.GetComponent<Camera>(*camera_).target = cameraGetAt();
}

CameraControl::~CameraControl() { cameraDestroy(); }

void CameraControl::SetCamera(Entity camera) { camera_ = camera; }

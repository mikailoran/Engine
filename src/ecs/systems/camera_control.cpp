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

void CameraControl::Init() {
  cameraCreate();
  cameraSetPosition(kStartPosition);
  cameraSetVerticalAngle(kStartVerticalAngle);
}

void CameraControl::Update(Ecs& ecs, const FrameContext& ctx) {
  if (ctx.mouse == nullptr) {
    return;
  }

  cameraUpdate(ctx.dt, *ctx.mouse);

  // Mirror the resulting pose into the components.
  const auto position = cameraGetPosition();
  const auto at = cameraGetAt();

  ecs.View<Transform, Camera>().ForEach(
      [&position, &at](Entity, Transform& transform, Camera& camera) {
        transform.position = position;
        camera.target = at;
      });
}

void CameraControl::Shutdown() { cameraDestroy(); }

#include "camera_control.h"

#include "../components/camera.h"
#include "../components/transform.h"
#include "../core/ecs.h"
#include "../core/frame_context.h"

#include <camera.h>
#include <entry/entry.h>

namespace {

// Starting eye position
constexpr bx::Vec3 kStartPosition{0.0F, 1.0F, -5.0F};

constexpr float kStartVerticalAngle = 0.0F;

} // namespace

void CameraControl::Init() {
  cameraCreate();
  cameraSetPosition(kStartPosition);
  cameraSetVerticalAngle(kStartVerticalAngle);
}

void CameraControl::Update(Ecs &ecs, const FrameContext &ctx) {
  if (ctx.mouse == nullptr) {
    return;
  }

  cameraUpdate(ctx.dt, *ctx.mouse);

  // Mirror the resulting pose into the components.
  const auto position = cameraGetPosition();
  const auto at = cameraGetAt();

  for (const auto &entity : entities) {
    auto &transform = ecs.GetComponent<Transform>(entity);
    auto &camera = ecs.GetComponent<Camera>(entity);

    transform.position = position;
    camera.target = at;
  }
}

void CameraControl::Shutdown() { cameraDestroy(); }

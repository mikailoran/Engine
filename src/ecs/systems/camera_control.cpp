#include "ecs/systems/camera_control.h"

#include <bgfx/bgfx.h>
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
  if (!camera_) {
    return;
  }

  auto& transform = ecs.GetComponent<Transform>(*camera_);
  auto& camera = ecs.GetComponent<Camera>(*camera_);

  if (ctx.mouse != nullptr) {
    cameraUpdate(ctx.dt, *ctx.mouse);

    // Mirror the resulting pose into the components.
    transform.position = cameraGetPosition();
    camera.target = cameraGetAt();
  }

  // Derive view and projection for this frame's readers
  const auto aspect =
      static_cast<float>(ctx.width) / static_cast<float>(ctx.height);
  bx::mtxLookAt(camera.view.data(), transform.position, camera.target,
                camera.up);
  bx::mtxProj(camera.proj.data(), camera.fov_degrees, aspect, camera.near_plane,
              camera.far_plane, bgfx::getCaps()->homogeneousDepth);
}

CameraControl::~CameraControl() { cameraDestroy(); }

void CameraControl::SetCamera(Entity camera) { camera_ = camera; }

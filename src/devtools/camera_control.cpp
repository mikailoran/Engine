#include "devtools/camera_control.h"

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <camera.h>
#include <entry/entry.h>

#include "engine/ecs/components/camera.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
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

  auto& transform = ecs.GetComponent<Transform>(*camera_);
  auto& camera = ecs.GetComponent<Camera>(*camera_);

  cameraUpdate(ctx.dt, mouse);

  // Mirror the resulting pose into the components.
  transform.position = cameraGetPosition();
  camera.target = cameraGetAt();

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

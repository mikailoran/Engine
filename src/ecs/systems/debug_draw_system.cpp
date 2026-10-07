#include "ecs/systems/debug_draw_system.h"

#include <bgfx/bgfx.h>
#include <bx/bounds.h>
#include <bx/math.h>
#include <debugdraw/debugdraw.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>

#include "ecs/components/camera.h"
#include "ecs/components/collider.h"
#include "ecs/components/renderable.h"
#include "ecs/components/rigid_body.h"
#include "ecs/components/transform.h"
#include "ecs/core/ecs.h"
#include "ecs/core/types.h"
#include "physics/shape.h"
#include "platform/frame_context.h"
#include "resource/asset_registry.h"

namespace {

// Rendered after view 0, sharing its depth buffer.
constexpr bgfx::ViewId kDebugView = 1;

constexpr std::uint32_t kGridColor = 0xff808080;       // ABGR
constexpr std::uint32_t kColliderColor = 0xff00ff00;   // ABGR
constexpr std::uint32_t kVelocityColor = 0xff00ffff;   // ABGR
constexpr std::uint32_t kHighlightColor = 0xff0080ff;  // ABGR
// Grows highlight boxes past the mesh so they don't sit on its faces
constexpr float kHighlightGrowth = 1.02F;
constexpr std::uint32_t kGridSize = 20;
constexpr float kGridStep = 1.0F;
// Keeps the grid off the floor's top face, which sits at y = 0
constexpr float kGridLift = 0.005F;
// Arrow length per m/s: the distance covered in this many seconds
constexpr float kVelocityArrowScale = 0.25F;
// Slower bodies get no arrow, hiding resting jitter
constexpr float kMinArrowSpeed = 0.05F;
constexpr float kArrowHeadLength = 0.15F;
constexpr float kArrowHeadRadius = 0.05F;

/** @brief Draws @p collider's shape as a wireframe, placed by @p transform. */
void DrawCollider(DebugDrawEncoder& encoder, const Transform& transform,
                  const Collider& collider) {
  const ShapeDesc& shape = collider.shape;
  const auto model = ModelMatrix(transform);

  switch (shape.kind) {
    case ShapeKind::kBox: {
      // Unit cube [-1, 1] -> shape space -> world
      std::array<float, 16> local{};
      bx::mtxSRT(local.data(), shape.half_extents.x, shape.half_extents.y,
                 shape.half_extents.z, 0.0F, 0.0F, 0.0F, shape.offset.x,
                 shape.offset.y, shape.offset.z);
      bx::Obb obb{};
      bx::mtxMul(std::data(obb.mtx), local.data(), model.data());
      encoder.draw(obb);
      break;
    }
    case ShapeKind::kSphere: {
      // Largest scale axis, as physics does; the model matrix would stretch it
      const bx::Vec3 size = bx::abs(transform.scale);
      const bx::Sphere sphere{
          .center = bx::mul(shape.offset, model.data()),
          .radius = shape.radius * std::max({size.x, size.y, size.z}),
      };
      encoder.draw(sphere);
      break;
    }
  }
}

/** @brief Draws an arrow from @p transform's origin along @p velocity. */
void DrawVelocityArrow(DebugDrawEncoder& encoder, const Transform& transform,
                       const bx::Vec3& velocity) {
  const float speed = bx::length(velocity);
  if (speed < kMinArrowSpeed) {
    return;
  }

  const bx::Vec3 dir = bx::mul(velocity, 1.0F / speed);
  const float length = speed * kVelocityArrowScale;
  const float head = std::min(kArrowHeadLength, length);
  const bx::Vec3 tip = bx::mad(dir, length, transform.position);
  const bx::Vec3 head_base = bx::mad(dir, -head, tip);

  encoder.moveTo(transform.position);
  encoder.lineTo(head_base);
  // Base at the first point, apex at the second
  encoder.drawCone(head_base, tip, kArrowHeadRadius);
}

/** @brief Draws a wire box around @p bounds, placed by @p transform. */
void DrawHighlight(DebugDrawEncoder& encoder, const Transform& transform,
                   const bx::Aabb& bounds) {
  const bx::Vec3 center = bx::mul(bx::add(bounds.min, bounds.max), 0.5F);
  const bx::Vec3 half =
      bx::mul(bx::sub(bounds.max, bounds.min), 0.5F * kHighlightGrowth);

  // Unit cube [-1, 1] -> mesh space -> world
  std::array<float, 16> local{};
  bx::mtxSRT(local.data(), half.x, half.y, half.z, 0.0F, 0.0F, 0.0F, center.x,
             center.y, center.z);
  bx::Obb obb{};
  bx::mtxMul(std::data(obb.mtx), local.data(), ModelMatrix(transform).data());
  encoder.draw(obb);
}

}  // namespace

DebugDrawSystem::DebugDrawSystem() { ddInit(); }

DebugDrawSystem::~DebugDrawSystem() { ddShutdown(); }

void DebugDrawSystem::Update(Ecs& ecs, const AssetRegistry& assets,
                             const FrameContext& ctx) {
  if (!camera_) {
    highlights_.clear();
    return;
  }

  const auto& camera = ecs.GetComponent<Camera>(*camera_);
  bgfx::setViewTransform(kDebugView, camera.view.data(), camera.proj.data());
  bgfx::setViewRect(kDebugView, 0, 0, static_cast<std::uint16_t>(ctx.width),
                    static_cast<std::uint16_t>(ctx.height));

  DebugDrawEncoder encoder;
  encoder.begin(kDebugView);

  encoder.drawAxis(0.0F, 0.0F, 0.0F);
  encoder.push();
  encoder.setColor(kGridColor);
  encoder.drawGrid(Axis::Y, {0.0F, kGridLift, 0.0F}, kGridSize, kGridStep);
  encoder.pop();

  encoder.push();
  encoder.setWireframe(true);
  encoder.setColor(kColliderColor);
  ecs.View<Transform, Collider>().ForEach(
      [&encoder](Entity /*entity*/, const Transform& transform,
                 const Collider& collider) -> void {
        DrawCollider(encoder, transform, collider);
      });
  encoder.pop();

  encoder.push();
  encoder.setColor(kVelocityColor);
  ecs.View<Transform, RigidBody>().ForEach(
      [&encoder](Entity /*entity*/, const Transform& transform,
                 const RigidBody& rigid_body) -> void {
        DrawVelocityArrow(encoder, transform, rigid_body.velocity);
      });
  encoder.pop();

  encoder.push();
  encoder.setWireframe(true);
  encoder.setColor(kHighlightColor);
  for (const Entity entity : highlights_) {
    if (!ecs.HasComponent<Transform>(entity) ||
        !ecs.HasComponent<Renderable>(entity)) {
      continue;
    }
    const auto& renderable = ecs.GetComponent<Renderable>(entity);
    DrawHighlight(encoder, ecs.GetComponent<Transform>(entity),
                  assets.GetMeshBounds(renderable.mesh_handle));
  }
  highlights_.clear();
  encoder.pop();

  encoder.end();
}

void DebugDrawSystem::Highlight(Entity entity) {
  highlights_.push_back(entity);
}

void DebugDrawSystem::SetCamera(Entity camera) { camera_ = camera; }

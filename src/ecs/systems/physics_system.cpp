#include "ecs/systems/physics_system.h"

#include <Jolt/Core/Core.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Geometry/AABox.h>
#include <Jolt/Math/MathTypes.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/EPhysicsUpdateError.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <bgfx_utils.h>
#include <bx/bounds.h>
#include <bx/math.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

#include "ecs/components/collider.h"
#include "ecs/components/renderable.h"
#include "ecs/components/rigid_body.h"
#include "ecs/components/transform.h"
#include "ecs/core/ecs.h"
#include "ecs/core/types.h"
#include "physics/layers.h"
#include "platform/frame_context.h"
#include "resource/asset_registry.h"

namespace {

// Jolt drops body pairs and contacts beyond these
constexpr JPH::uint kMaxBodyPairs = 65536;
constexpr JPH::uint kMaxContactConstraints = 10240;

/// Below this cos(y), X and Z rotate about the same axis (gimbal lock).
constexpr float kGimbalEpsilon = 1e-6F;

/// How far past a body's bounds WakeTouching looks for neighbours, in m.
constexpr float kWakeMargin = 0.1F;

/** @brief Converts a bx vector to Jolt's. */
auto ToJolt(const bx::Vec3& v) -> JPH::Vec3 { return {v.x, v.y, v.z}; }

/** @brief Converts a Jolt vector to bx's. */
auto ToBx(JPH::Vec3Arg v) -> bx::Vec3 { return {v.GetX(), v.GetY(), v.GetZ()}; }

/** @brief Tests two vectors for exact equality. */
auto Same(const bx::Vec3& a, const bx::Vec3& b) -> bool {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

/** @brief Converts Transform's Euler angles to the rotation Jolt applies. */
auto ToJoltRotation(const bx::Vec3& euler) -> JPH::Quat {
  // bx's row-vector matrices turn the other way, hence the conjugate
  const bx::Quaternion q = bx::fromEuler(euler);
  return {-q.x, -q.y, -q.z, q.w};
}

/**
 * @brief Converts a Jolt rotation to Euler angles that bx::mtxSRT reproduces.
 *
 * bx::toEuler does not invert bx::fromEuler, so the angles are read off the
 * rotated axes, matching mtxSRT's matrix terms.
 */
auto ToEuler(JPH::QuatArg rotation) -> bx::Vec3 {
  const JPH::Vec3 x_axis = rotation.RotateAxisX();
  const JPH::Vec3 y_axis = rotation.RotateAxisY();
  const JPH::Vec3 z_axis = rotation.RotateAxisZ();
  const float sin_y = -z_axis.GetX();
  const float cos_y = std::hypot(x_axis.GetX(), y_axis.GetX());
  const float y = std::atan2(sin_y, cos_y);
  // Gimbal lock: only X + Z is defined, so fold it all into X
  if (cos_y < kGimbalEpsilon) {
    return {std::atan2(sin_y * x_axis.GetY(), y_axis.GetY()), y, 0.0F};
  }
  return {std::atan2(z_axis.GetY(), z_axis.GetZ()), y,
          std::atan2(y_axis.GetX(), x_axis.GetX())};
}

/**
 * @brief Wakes @p id's body and every body touching it.
 *
 * Static bodies never wake, so for a floor this wakes what rests on it.
 */
void WakeTouching(JPH::BodyInterface& body_interface, const JPH::BodyID& id) {
  JPH::AABox bounds =
      body_interface.GetTransformedShape(id).GetWorldSpaceBounds();
  bounds.ExpandBy(JPH::Vec3::sReplicate(kWakeMargin));
  body_interface.ActivateBodiesInAABox(bounds, JPH::BroadPhaseLayerFilter{},
                                       JPH::ObjectLayerFilter{});
}

/** @brief Returns the box around all of @p mesh's groups, in mesh space. */
auto MeshBounds(const Mesh& mesh) -> bx::Aabb {
  assert(!mesh.m_groups.empty() && "mesh has no groups");
  // Inverted, so an empty mesh degrades to a minimal box below
  constexpr float kMax = std::numeric_limits<float>::max();
  bx::Aabb bounds{.min = {kMax, kMax, kMax}, .max = {-kMax, -kMax, -kMax}};
  for (const Group& group : mesh.m_groups) {
    bounds.min = bx::min(bounds.min, group.m_aabb.min);
    bounds.max = bx::max(bounds.max, group.m_aabb.max);
  }
  return bounds;
}

/**
 * @brief Builds a box around @p bounds, scaled by @p scale.
 * @return The shape, or null if Jolt rejected it.
 */
auto MakeBoxShape(const bx::Aabb& bounds, const bx::Vec3& scale)
    -> JPH::RefConst<JPH::Shape> {
  const bx::Vec3 size =
      bx::mul(bx::sub(bounds.max, bounds.min), bx::abs(scale));
  // Each half extent must cover the box's rounded edges
  const JPH::Vec3 half_extent =
      JPH::Vec3::sMax(ToJolt(bx::mul(size, 0.5F)),
                      JPH::Vec3::sReplicate(JPH::cDefaultConvexRadius));
  const JPH::Vec3 center =
      ToJolt(bx::mul(bx::mul(bx::add(bounds.min, bounds.max), 0.5F), scale));

  // Stack-owned settings, so mark them embedded for Jolt's ref counting
  JPH::BoxShapeSettings box(half_extent);
  box.SetEmbedded();
  const JPH::RotatedTranslatedShapeSettings offset(
      center, JPH::Quat::sIdentity(), &box);
  offset.SetEmbedded();

  // Most meshes are centered, which needs no offset wrapper
  const JPH::ShapeSettings& settings =
      center.IsNearZero() ? static_cast<const JPH::ShapeSettings&>(box)
                          : offset;
  const JPH::ShapeSettings::ShapeResult result = settings.Create();
  assert(result.IsValid() && "Jolt rejected a box shape");
  return result.IsValid() ? result.Get() : nullptr;
}

}  // namespace

Physics::Physics() {
  // 0 body mutexes lets Jolt pick a default
  const auto num_body_mutexes = 0U;
  world_.Init(static_cast<JPH::uint>(kMaxEntities), num_body_mutexes,
              kMaxBodyPairs, kMaxContactConstraints, layers_.BroadPhase(),
              layers_.ObjectVsBroadPhase(), layers_.ObjectPairs());
}

void Physics::Update(Ecs& ecs, const AssetRegistry& assets,
                     const FrameContext& ctx) {
  RemoveStaleBodies(ecs);
  CreateBodies(ecs, assets);
  PushEdits(ecs);

  // A long hitch would otherwise queue more steps than the frame can afford.
  accumulator_ += std::min(ctx.dt, kMaxFrameDt);

  // Drain whole steps only; the remainder carries to the next frame.
  while (accumulator_ >= kFixedDt) {
    ApplyAccelerations(ecs);
    [[maybe_unused]] const JPH::EPhysicsUpdateError error =
        world_.Update(kFixedDt, 1, &temp_allocator_, &job_system_);
    assert(error == JPH::EPhysicsUpdateError::None &&
           "Jolt ran out of body pairs or contacts");
    accumulator_ -= kFixedDt;
  }

  ReadBack(ecs);
}

void Physics::RemoveStaleBodies(Ecs& ecs) {
  JPH::BodyInterface& body_interface = world_.GetBodyInterface();
  for (auto it = bodies_.begin(); it != bodies_.end();) {
    const auto& [entity, record] = *it;
    const std::uint32_t id = record.id.GetIndexAndSequenceNumber();
    auto* collider = ecs.TryGetComponent<Collider>(entity);
    const auto* transform = ecs.TryGetComponent<Transform>(entity);

    // A mismatched id means the entity was reused or re-added its Collider
    const bool stale = collider == nullptr || transform == nullptr ||
                       !ecs.HasComponent<Renderable>(entity) ||
                       collider->body_id != id ||
                       ecs.HasComponent<RigidBody>(entity) != record.dynamic ||
                       !Same(transform->scale, record.scale);
    if (!stale) {
      ++it;
      continue;
    }

    body_interface.RemoveBody(record.id);
    body_interface.DestroyBody(record.id);
    // Lets CreateBodies rebuild it if the entity still qualifies
    if (collider != nullptr && collider->body_id == id) {
      collider->body_id = Collider::kNoBody;
    }
    it = bodies_.erase(it);
  }
}

void Physics::CreateBodies(Ecs& ecs, const AssetRegistry& assets) {
  JPH::BodyInterface& body_interface = world_.GetBodyInterface();
  ecs.View<Transform, Renderable, Collider>().ForEach(
      [&](Entity entity, const Transform& transform,
          const Renderable& renderable, Collider& collider) -> void {
        if (collider.body_id != Collider::kNoBody) {
          return;
        }
        assert(!bodies_.contains(entity) && "stale record not removed");

        const auto shape =
            MakeBoxShape(MeshBounds(*assets.GetMesh(renderable.mesh_handle)),
                         transform.scale);
        if (!shape) {
          return;
        }

        const auto* rigid_body = ecs.TryGetComponent<RigidBody>(entity);
        const bool is_dynamic = rigid_body != nullptr;
        JPH::BodyCreationSettings settings{
            shape, ToJolt(transform.position),
            ToJoltRotation(transform.rotation),
            is_dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
            is_dynamic ? object_layer::kMoving : object_layer::kNonMoving};
        const bool has_gravity = is_dynamic && rigid_body->has_gravity;
        const bx::Vec3 velocity =
            is_dynamic ? rigid_body->velocity : bx::Vec3{0.0F};
        settings.mRestitution = collider.restitution;
        settings.mFriction = collider.friction;
        settings.mGravityFactor = has_gravity ? 1.0F : 0.0F;
        settings.mLinearVelocity = ToJolt(velocity);

        const JPH::BodyID id = body_interface.CreateAndAddBody(
            settings, is_dynamic ? JPH::EActivation::Activate
                                 : JPH::EActivation::DontActivate);
        assert(!id.IsInvalid() && "Jolt is out of bodies");
        if (id.IsInvalid()) {
          return;
        }
        collider.body_id = id.GetIndexAndSequenceNumber();
        bodies_.emplace(entity, BodyRecord{.id = id,
                                           .dynamic = is_dynamic,
                                           .has_gravity = has_gravity,
                                           .scale = transform.scale,
                                           .position = transform.position,
                                           .rotation = transform.rotation,
                                           .velocity = velocity,
                                           .restitution = collider.restitution,
                                           .friction = collider.friction});
      });
}

void Physics::PushEdits(Ecs& ecs) {
  JPH::BodyInterface& body_interface = world_.GetBodyInterface();
  for (auto& [entity, record] : bodies_) {
    const auto& transform = ecs.GetComponent<Transform>(entity);
    if (!Same(transform.position, record.position) ||
        !Same(transform.rotation, record.rotation)) {
      body_interface.SetPositionAndRotation(
          record.id, ToJolt(transform.position),
          ToJoltRotation(transform.rotation),
          record.dynamic ? JPH::EActivation::Activate
                         : JPH::EActivation::DontActivate);
      record.position = transform.position;
      record.rotation = transform.rotation;
    }
    const auto& collider = ecs.GetComponent<Collider>(entity);
    if (collider.restitution != record.restitution ||
        collider.friction != record.friction) {
      body_interface.SetRestitution(record.id, collider.restitution);
      body_interface.SetFriction(record.id, collider.friction);
      record.restitution = collider.restitution;
      record.friction = collider.friction;
      // Neither setter wakes anything, so a resting body would ignore the edit
      WakeTouching(body_interface, record.id);
    }

    if (!record.dynamic) {
      continue;
    }
    const auto& rigid_body = ecs.GetComponent<RigidBody>(entity);
    if (!Same(rigid_body.velocity, record.velocity)) {
      body_interface.SetLinearVelocity(record.id, ToJolt(rigid_body.velocity));
      record.velocity = rigid_body.velocity;
    }
    if (rigid_body.has_gravity != record.has_gravity) {
      body_interface.SetGravityFactor(record.id,
                                      rigid_body.has_gravity ? 1.0F : 0.0F);
      // Unlike a velocity change, this does not wake a sleeping body
      body_interface.ActivateBody(record.id);
      record.has_gravity = rigid_body.has_gravity;
    }
  }
}

void Physics::ApplyAccelerations(Ecs& ecs) {
  JPH::BodyInterface& body_interface = world_.GetBodyInterface();
  for (const auto& [entity, record] : bodies_) {
    if (!record.dynamic) {
      continue;
    }
    const bx::Vec3& acceleration =
        ecs.GetComponent<RigidBody>(entity).acceleration;
    // Skipping zero keeps sleeping bodies asleep
    if (!Same(acceleration, bx::Vec3{0.0F})) {
      body_interface.AddLinearVelocity(record.id,
                                       ToJolt(bx::mul(acceleration, kFixedDt)));
    }
  }
}

void Physics::ReadBack(Ecs& ecs) {
  const JPH::BodyInterface& body_interface = world_.GetBodyInterface();
  for (auto& [entity, record] : bodies_) {
    if (!record.dynamic) {
      continue;
    }
    JPH::RVec3 position = JPH::RVec3::sZero();
    JPH::Quat rotation = JPH::Quat::sIdentity();
    body_interface.GetPositionAndRotation(record.id, position, rotation);

    auto& transform = ecs.GetComponent<Transform>(entity);
    auto& rigid_body = ecs.GetComponent<RigidBody>(entity);
    transform.position = ToBx(position);
    transform.rotation = ToEuler(rotation);
    rigid_body.velocity = ToBx(body_interface.GetLinearVelocity(record.id));

    record.position = transform.position;
    record.rotation = transform.rotation;
    record.velocity = rigid_body.velocity;
  }
}

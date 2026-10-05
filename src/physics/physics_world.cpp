#include "physics/physics_world.h"

// The header brings no Jolt, and every other Jolt header needs it first
#include <Jolt/Jolt.h>  // IWYU pragma: keep
#include <Jolt/Core/Core.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Geometry/AABox.h>
#include <Jolt/Math/MathTypes.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/EPhysicsUpdateError.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <bx/math.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "physics/body_handle.h"
#include "physics/jolt_runtime.h"
#include "physics/layers.h"
#include "physics/shape.h"

namespace {

// Jolt drops body pairs and contacts beyond these
constexpr JPH::uint kMaxBodyPairs = 65536;
constexpr JPH::uint kMaxContactConstraints = 10240;

/// Scratch memory Jolt uses within one step, in bytes.
constexpr JPH::uint kTempAllocatorBytes = 10U * 1024U * 1024U;

/// How far past a body's bounds WakeTouching looks for neighbours, in m.
constexpr float kWakeMargin = 0.1F;

/** @brief Converts a bx vector to Jolt's. */
auto ToJolt(const bx::Vec3& v) -> JPH::Vec3 { return {v.x, v.y, v.z}; }

/** @brief Converts a Jolt vector to bx's. */
auto ToBx(JPH::Vec3Arg v) -> bx::Vec3 { return {v.GetX(), v.GetY(), v.GetZ()}; }

/** @brief Converts a bx quaternion to Jolt's; both use v' = q v q*. */
auto ToJolt(const bx::Quaternion& q) -> JPH::Quat {
  return {q.x, q.y, q.z, q.w};
}

/** @brief Converts a Jolt quaternion to bx's. */
auto ToBx(JPH::QuatArg q) -> bx::Quaternion {
  return {q.GetX(), q.GetY(), q.GetZ(), q.GetW()};
}

/** @brief Converts an engine handle to Jolt's body id. */
auto ToJolt(BodyHandle body) -> JPH::BodyID {
  return JPH::BodyID(body.Value());
}

/** @brief Tests a vector for exact zero. */
auto IsZero(const bx::Vec3& v) -> bool {
  return v.x == 0.0F && v.y == 0.0F && v.z == 0.0F;
}

/** @brief Builds the inner shape of @p desc, centred on the origin. */
auto MakeCentredShape(const ShapeDesc& desc)
    -> JPH::ShapeSettings::ShapeResult {
  if (desc.kind == ShapeKind::kSphere) {
    assert(desc.radius > 0.0F && "sphere radius must be positive");
    return JPH::SphereShapeSettings(desc.radius).Create();
  }
  // Each half extent must cover the box's rounded edges
  const JPH::Vec3 half_extent =
      JPH::Vec3::sMax(ToJolt(desc.half_extents),
                      JPH::Vec3::sReplicate(JPH::cDefaultConvexRadius));
  return JPH::BoxShapeSettings(half_extent).Create();
}

/** @brief @p desc with @p scale applied to its sizes and offset. */
auto Scaled(const ShapeDesc& desc, const bx::Vec3& scale) -> ShapeDesc {
  const bx::Vec3 size = bx::abs(scale);
  ShapeDesc scaled = desc;
  scaled.half_extents = bx::mul(desc.half_extents, size);
  // Spheres cannot stretch, so the largest axis wins
  scaled.radius = desc.radius * std::max({size.x, size.y, size.z});
  scaled.offset = bx::mul(desc.offset, scale);
  return scaled;
}

/**
 * @brief Builds the Jolt shape for @p desc at @p scale.
 * @return The shape, or null if Jolt rejected it.
 */
auto MakeShape(const ShapeDesc& unscaled, const bx::Vec3& scale)
    -> JPH::RefConst<JPH::Shape> {
  const ShapeDesc desc = Scaled(unscaled, scale);
  JPH::ShapeSettings::ShapeResult result = MakeCentredShape(desc);
  // Most shapes are centred, which needs no offset wrapper
  if (result.IsValid() && !IsZero(desc.offset)) {
    result = JPH::RotatedTranslatedShapeSettings(
                 ToJolt(desc.offset), JPH::Quat::sIdentity(), result.Get())
                 .Create();
  }
  assert(result.IsValid() && "Jolt rejected a shape");
  return result.IsValid() ? result.Get() : nullptr;
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

/** @brief Wakes a dynamic body on an edit; static bodies cannot wake. */
auto ActivationFor(const JPH::BodyInterface& body_interface,
                   const JPH::BodyID& id) -> JPH::EActivation {
  return body_interface.GetMotionType(id) == JPH::EMotionType::Dynamic
             ? JPH::EActivation::Activate
             : JPH::EActivation::DontActivate;
}

}  // namespace

/** @brief Jolt's world and the helpers it needs while stepping. */
struct PhysicsWorld::Impl {
  // The world references the layers, so it must follow them
  CollisionLayers layers;
  JPH::PhysicsSystem system;
  JPH::TempAllocatorImpl temp_allocator{kTempAllocatorBytes};
  /// Per-body extra accelerations; Jolt has no persistent force.
  std::unordered_map<std::uint32_t, bx::Vec3> accelerations;
  // Last, where its 64-byte alignment adds no padding. One thread per core
  // but one by default
  // TODO: learn about job systems and how to integrate ours to Jolt's
  JPH::JobSystemThreadPool job_system{JPH::cMaxPhysicsJobs,
                                      JPH::cMaxPhysicsBarriers};
};

PhysicsWorld::PhysicsWorld(const JoltRuntime& /*runtime*/,
                           std::uint32_t max_bodies)
    : impl_(std::make_unique<Impl>()) {
  // 0 body mutexes lets Jolt pick a default
  const auto num_body_mutexes = 0U;
  impl_->system.Init(max_bodies, num_body_mutexes, kMaxBodyPairs,
                     kMaxContactConstraints, impl_->layers.BroadPhase(),
                     impl_->layers.ObjectVsBroadPhase(),
                     impl_->layers.ObjectPairs());
}

PhysicsWorld::~PhysicsWorld() = default;

auto PhysicsWorld::CreateBody(const BodyDesc& desc) -> BodyHandle {
  const auto shape = MakeShape(desc.shape, desc.scale);
  if (!shape) {
    return {};
  }

  const bool dynamic = desc.motion == Motion::kDynamic;
  JPH::BodyCreationSettings settings{
      shape, ToJolt(desc.pose.position), ToJolt(desc.pose.rotation),
      dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
      dynamic ? object_layer::kMoving : object_layer::kNonMoving};
  settings.mRestitution = desc.material.restitution;
  settings.mFriction = desc.material.friction;
  settings.mGravityFactor = desc.gravity ? 1.0F : 0.0F;
  settings.mLinearVelocity = ToJolt(desc.velocity);
  settings.mUserData = desc.user_data;
  // Lets a static body turn dynamic in place
  settings.mAllowDynamicOrKinematic = true;

  const JPH::BodyID id = impl_->system.GetBodyInterface().CreateAndAddBody(
      settings,
      dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
  assert(!id.IsInvalid() && "Jolt is out of bodies");
  return BodyHandle(id.GetIndexAndSequenceNumber());
}

void PhysicsWorld::DestroyBody(BodyHandle body) {
  JPH::BodyInterface& body_interface = impl_->system.GetBodyInterface();
  const JPH::BodyID id = ToJolt(body);
  // Whatever rested on it would otherwise sleep on in mid-air
  WakeTouching(body_interface, id);
  body_interface.RemoveBody(id);
  body_interface.DestroyBody(id);
  impl_->accelerations.erase(body.Value());
}

void PhysicsWorld::SetPose(BodyHandle body, const Pose& pose) {
  JPH::BodyInterface& body_interface = impl_->system.GetBodyInterface();
  const JPH::BodyID id = ToJolt(body);
  // Wake what touches the old place, then the new one
  WakeTouching(body_interface, id);
  body_interface.SetPositionAndRotation(id, ToJolt(pose.position),
                                        ToJolt(pose.rotation),
                                        ActivationFor(body_interface, id));
  WakeTouching(body_interface, id);
}

void PhysicsWorld::SetShape(BodyHandle body, const ShapeDesc& shape,
                            const bx::Vec3& scale) {
  const auto jolt_shape = MakeShape(shape, scale);
  if (!jolt_shape) {
    return;
  }
  JPH::BodyInterface& body_interface = impl_->system.GetBodyInterface();
  const JPH::BodyID id = ToJolt(body);
  // Wake what touches the old shape, then the new one
  WakeTouching(body_interface, id);
  body_interface.SetShape(id, jolt_shape, true,
                          ActivationFor(body_interface, id));
  WakeTouching(body_interface, id);
}

void PhysicsWorld::SetMaterial(BodyHandle body, const Material& material) {
  JPH::BodyInterface& body_interface = impl_->system.GetBodyInterface();
  const JPH::BodyID id = ToJolt(body);
  body_interface.SetRestitution(id, material.restitution);
  body_interface.SetFriction(id, material.friction);
  // Neither setter wakes anything, so a resting body would ignore the edit
  WakeTouching(body_interface, id);
}

void PhysicsWorld::SetMotion(BodyHandle body, Motion motion) {
  JPH::BodyInterface& body_interface = impl_->system.GetBodyInterface();
  const JPH::BodyID id = ToJolt(body);
  const bool dynamic = motion == Motion::kDynamic;
  body_interface.SetObjectLayer(
      id, dynamic ? object_layer::kMoving : object_layer::kNonMoving);
  body_interface.SetMotionType(
      id, dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
      JPH::EActivation::Activate);
}

void PhysicsWorld::SetVelocity(BodyHandle body, const bx::Vec3& velocity) {
  impl_->system.GetBodyInterface().SetLinearVelocity(ToJolt(body),
                                                     ToJolt(velocity));
}

void PhysicsWorld::SetGravityEnabled(BodyHandle body, bool enabled) {
  JPH::BodyInterface& body_interface = impl_->system.GetBodyInterface();
  const JPH::BodyID id = ToJolt(body);
  body_interface.SetGravityFactor(id, enabled ? 1.0F : 0.0F);
  // Unlike a velocity change, this does not wake a sleeping body
  body_interface.ActivateBody(id);
}

void PhysicsWorld::SetAcceleration(BodyHandle body,
                                   const bx::Vec3& acceleration) {
  if (IsZero(acceleration)) {
    impl_->accelerations.erase(body.Value());
  } else {
    impl_->accelerations.insert_or_assign(body.Value(), acceleration);
  }
}

auto PhysicsWorld::GetPose(BodyHandle body) const -> Pose {
  JPH::RVec3 position = JPH::RVec3::sZero();
  JPH::Quat rotation = JPH::Quat::sIdentity();
  impl_->system.GetBodyInterface().GetPositionAndRotation(ToJolt(body),
                                                          position, rotation);
  return {.position = ToBx(position), .rotation = ToBx(rotation)};
}

auto PhysicsWorld::GetVelocity(BodyHandle body) const -> bx::Vec3 {
  return ToBx(impl_->system.GetBodyInterface().GetLinearVelocity(ToJolt(body)));
}

// TODO: if the per-frame sync shows in profiles, read via
// GetBodyInterfaceNoLock() here and in Bodies(); safe between Steps
auto PhysicsWorld::UserData(BodyHandle body) const
    -> std::optional<std::uint64_t> {
  const JPH::BodyInterface& body_interface = impl_->system.GetBodyInterface();
  const JPH::BodyID id = ToJolt(body);
  // A destroyed body's id fails the lock, even if its slot was reused
  if (!body.IsValid() || !body_interface.IsAdded(id)) {
    return std::nullopt;
  }
  return body_interface.GetUserData(id);
}

auto PhysicsWorld::Bodies() const -> std::vector<BodyEntry> {
  JPH::BodyIDVector ids;
  impl_->system.GetBodies(ids);
  const JPH::BodyInterface& body_interface = impl_->system.GetBodyInterface();
  std::vector<BodyEntry> bodies;
  bodies.reserve(ids.size());
  for (const JPH::BodyID& id : ids) {
    bodies.push_back({.body = BodyHandle(id.GetIndexAndSequenceNumber()),
                      .user_data = body_interface.GetUserData(id)});
  }
  return bodies;
}

void PhysicsWorld::Step(float dt) {
  JPH::BodyInterface& body_interface = impl_->system.GetBodyInterface();
  for (const auto& [value, acceleration] : impl_->accelerations) {
    const JPH::BodyID id(value);
    if (body_interface.GetMotionType(id) == JPH::EMotionType::Dynamic) {
      body_interface.AddLinearVelocity(id, ToJolt(bx::mul(acceleration, dt)));
    }
  }

  [[maybe_unused]] const JPH::EPhysicsUpdateError error =
      impl_->system.Update(dt, 1, &impl_->temp_allocator, &impl_->job_system);
  assert(error == JPH::EPhysicsUpdateError::None &&
         "Jolt ran out of body pairs or contacts");
}

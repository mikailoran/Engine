#include "ecs/systems/physics_system.h"

#include <bgfx_utils.h>
#include <bx/bounds.h>
#include <bx/math.h>

#include <algorithm>
#include <cassert>
#include <limits>

#include "ecs/components/collider.h"
#include "ecs/components/renderable.h"
#include "ecs/components/rigid_body.h"
#include "ecs/components/transform.h"
#include "ecs/core/ecs.h"
#include "ecs/core/types.h"
#include "physics/physics_world.h"
#include "platform/frame_context.h"
#include "resource/asset_registry.h"

namespace {

/** @brief Tests two vectors for exact equality. */
auto Same(const bx::Vec3& a, const bx::Vec3& b) -> bool {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

/** @brief Tests two quaternions for exact equality. */
auto Same(const bx::Quaternion& a, const bx::Quaternion& b) -> bool {
  return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
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

/** @brief The box around @p mesh, scaled by @p scale, in body space. */
auto FitBox(const Mesh& mesh, const bx::Vec3& scale) -> ShapeDesc {
  const bx::Aabb bounds = MeshBounds(mesh);
  const bx::Vec3 size =
      bx::mul(bx::sub(bounds.max, bounds.min), bx::abs(scale));
  const bx::Vec3 center = bx::mul(bx::add(bounds.min, bounds.max), 0.5F);
  return {.half_extents = bx::mul(size, 0.5F),
          .offset = bx::mul(center, scale)};
}

/** @brief The surface response @p collider asks for. */
auto MaterialOf(const Collider& collider) -> Material {
  return {.restitution = collider.restitution, .friction = collider.friction};
}

}  // namespace

void Physics::Update(Ecs& ecs, PhysicsWorld& world, const AssetRegistry& assets,
                     const FrameContext& ctx) {
  RemoveStaleBodies(ecs, world);
  CreateBodies(ecs, world, assets);
  PushEdits(ecs, world, assets);

  // A long hitch would otherwise queue more steps than the frame can afford.
  accumulator_ += std::min(ctx.dt, kMaxFrameDt);

  // Drain whole steps only; the remainder carries to the next frame.
  while (accumulator_ >= kFixedDt) {
    world.Step(kFixedDt);
    accumulator_ -= kFixedDt;
  }

  ReadBack(ecs, world);
}

void Physics::RemoveStaleBodies(Ecs& ecs, PhysicsWorld& world) {
  for (auto it = bodies_.begin(); it != bodies_.end();) {
    const auto& [entity, record] = *it;
    auto* collider = ecs.TryGetComponent<Collider>(entity);
    const auto* transform = ecs.TryGetComponent<Transform>(entity);

    // A mismatched id means the entity was reused or re-added its Collider
    const bool stale = collider == nullptr || transform == nullptr ||
                       !ecs.HasComponent<Renderable>(entity) ||
                       collider->body_id != record.body.Value();
    if (!stale) {
      ++it;
      continue;
    }

    world.DestroyBody(record.body);
    // Lets CreateBodies rebuild it if the entity still qualifies
    if (collider != nullptr && collider->body_id == record.body.Value()) {
      collider->body_id = Collider::kNoBody;
    }
    it = bodies_.erase(it);
  }
}

void Physics::CreateBodies(Ecs& ecs, PhysicsWorld& world,
                           const AssetRegistry& assets) {
  ecs.View<Transform, Renderable, Collider>().ForEach(
      [&](Entity entity, const Transform& transform,
          const Renderable& renderable, Collider& collider) -> void {
        if (collider.body_id != Collider::kNoBody) {
          return;
        }
        assert(!bodies_.contains(entity) && "stale record not removed");

        const auto* rigid_body = ecs.TryGetComponent<RigidBody>(entity);
        const bool is_dynamic = rigid_body != nullptr;
        const bool has_gravity = is_dynamic && rigid_body->has_gravity;
        const bx::Vec3 velocity =
            is_dynamic ? rigid_body->velocity : bx::Vec3{0.0F};
        const bx::Vec3 acceleration =
            is_dynamic ? rigid_body->acceleration : bx::Vec3{0.0F};

        const BodyHandle body = world.CreateBody(
            BodyDesc{.shape = FitBox(*assets.GetMesh(renderable.mesh_handle),
                                     transform.scale),
                     .pose = {.position = transform.position,
                              .rotation = transform.rotation},
                     .motion = is_dynamic ? Motion::kDynamic : Motion::kStatic,
                     .material = MaterialOf(collider),
                     .velocity = velocity,
                     .gravity = has_gravity});
        if (!body.IsValid()) {
          return;
        }
        world.SetAcceleration(body, acceleration);
        collider.body_id = body.Value();
        bodies_.emplace(entity, BodyRecord{.body = body,
                                           .dynamic = is_dynamic,
                                           .has_gravity = has_gravity,
                                           .scale = transform.scale,
                                           .position = transform.position,
                                           .rotation = transform.rotation,
                                           .velocity = velocity,
                                           .acceleration = acceleration,
                                           .restitution = collider.restitution,
                                           .friction = collider.friction});
      });
}

void Physics::PushEdits(Ecs& ecs, PhysicsWorld& world,
                        const AssetRegistry& assets) {
  for (auto& [entity, record] : bodies_) {
    const Renderable& renderable = ecs.GetComponent<Renderable>(entity);
    PushPoseAndShape(world, record, ecs.GetComponent<Transform>(entity),
                     *assets.GetMesh(renderable.mesh_handle));
    PushMaterial(world, record, ecs.GetComponent<Collider>(entity));
    PushMotion(world, record, ecs.TryGetComponent<RigidBody>(entity));
  }
}

void Physics::PushPoseAndShape(PhysicsWorld& world, BodyRecord& record,
                               const Transform& transform, const Mesh& mesh) {
  if (!Same(transform.position, record.position) ||
      !Same(transform.rotation, record.rotation)) {
    world.SetPose(record.body, {.position = transform.position,
                                .rotation = transform.rotation});
    record.position = transform.position;
    record.rotation = transform.rotation;
  }
  if (!Same(transform.scale, record.scale)) {
    world.SetShape(record.body, FitBox(mesh, transform.scale));
    record.scale = transform.scale;
  }
}

void Physics::PushMaterial(PhysicsWorld& world, BodyRecord& record,
                           const Collider& collider) {
  if (collider.restitution == record.restitution &&
      collider.friction == record.friction) {
    return;
  }
  world.SetMaterial(record.body, MaterialOf(collider));
  record.restitution = collider.restitution;
  record.friction = collider.friction;
}

void Physics::PushMotion(PhysicsWorld& world, BodyRecord& record,
                         const RigidBody* rigid_body) {
  const bool is_dynamic = rigid_body != nullptr;
  if (is_dynamic != record.dynamic) {
    // A RigidBody added or removed switches the body's motion in place
    world.SetMotion(record.body,
                    is_dynamic ? Motion::kDynamic : Motion::kStatic);
    record.dynamic = is_dynamic;
    if (is_dynamic) {
      // Static bodies were built without gravity, velocity or acceleration
      world.SetGravityEnabled(record.body, rigid_body->has_gravity);
      world.SetVelocity(record.body, rigid_body->velocity);
      world.SetAcceleration(record.body, rigid_body->acceleration);
      record.has_gravity = rigid_body->has_gravity;
      record.velocity = rigid_body->velocity;
      record.acceleration = rigid_body->acceleration;
    }
    return;
  }
  if (!is_dynamic) {
    return;
  }

  if (!Same(rigid_body->velocity, record.velocity)) {
    world.SetVelocity(record.body, rigid_body->velocity);
    record.velocity = rigid_body->velocity;
  }
  if (rigid_body->has_gravity != record.has_gravity) {
    world.SetGravityEnabled(record.body, rigid_body->has_gravity);
    record.has_gravity = rigid_body->has_gravity;
  }
  if (!Same(rigid_body->acceleration, record.acceleration)) {
    world.SetAcceleration(record.body, rigid_body->acceleration);
    record.acceleration = rigid_body->acceleration;
  }
}

void Physics::ReadBack(Ecs& ecs, const PhysicsWorld& world) {
  for (auto& [entity, record] : bodies_) {
    if (!record.dynamic) {
      continue;
    }
    const Pose pose = world.GetPose(record.body);
    auto& transform = ecs.GetComponent<Transform>(entity);
    auto& rigid_body = ecs.GetComponent<RigidBody>(entity);
    transform.position = pose.position;
    transform.rotation = pose.rotation;
    rigid_body.velocity = world.GetVelocity(record.body);

    record.position = transform.position;
    record.rotation = transform.rotation;
    record.velocity = rigid_body.velocity;
  }
}

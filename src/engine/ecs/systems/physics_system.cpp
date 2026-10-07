#include "engine/ecs/systems/physics_system.h"

#include <bx/math.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>
#include <vector>

#include "engine/ecs/components/collider.h"
#include "engine/ecs/components/physics_link.h"
#include "engine/ecs/components/rigid_body.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/physics/body_handle.h"
#include "engine/physics/physics_world.h"
#include "engine/physics/shape.h"
#include "engine/platform/frame_context.h"

namespace {

/** @brief Tests two vectors for exact equality. */
auto Same(const bx::Vec3& a, const bx::Vec3& b) -> bool {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

/** @brief Tests two quaternions for exact equality. */
auto Same(const bx::Quaternion& a, const bx::Quaternion& b) -> bool {
  return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

/** @brief Tests whether two shapes are identical. */
auto Same(const ShapeDesc& a, const ShapeDesc& b) -> bool {
  return a.kind == b.kind && Same(a.half_extents, b.half_extents) &&
         a.radius == b.radius && Same(a.offset, b.offset);
}

/** @brief Tests whether two materials are identical. */
auto Same(const Material& a, const Material& b) -> bool {
  return a.restitution == b.restitution && a.friction == b.friction;
}

/** @brief The tag a body carries to name its entity. */
auto Tag(Entity entity) -> std::uint64_t {
  return static_cast<std::uint64_t>(entity);
}

}  // namespace

void PhysicsSystem::SweepOrphans(Ecs& ecs, PhysicsWorld& world) {
  for (const BodyEntry& entry : world.Bodies()) {
    const auto* link =
        ecs.TryGetComponent<PhysicsLink>(static_cast<Entity>(entry.user_data));
    if (link == nullptr || link->body_ != entry.body) {
      world.DestroyBody(entry.body);
    }
  }
}

void PhysicsSystem::DetachBodies(Ecs& ecs, PhysicsWorld& world) {
  std::vector<Entity> unlinked;
  ecs.View<PhysicsLink>().ForEach(
      [&](Entity entity, const PhysicsLink& link) -> void {
        const bool owns = world.UserData(link.body_) == Tag(entity);
        if (owns && ecs.HasComponent<Collider>(entity) &&
            ecs.HasComponent<Transform>(entity)) {
          return;
        }
        if (owns) {
          world.DestroyBody(link.body_);
        }
        unlinked.push_back(entity);
      });
  // Removing a viewed component inside ForEach asserts, so remove after
  for (const Entity entity : unlinked) {
    ecs.RemoveComponent<PhysicsLink>(entity);
  }
}

void PhysicsSystem::AttachBodies(Ecs& ecs, PhysicsWorld& world) {
  std::vector<Entity> unlinked;
  ecs.View<Collider, Transform>().ForEach(
      [&](Entity entity, const Collider&, const Transform&) -> void {
        if (!ecs.HasComponent<PhysicsLink>(entity)) {
          unlinked.push_back(entity);
        }
      });

  for (const Entity entity : unlinked) {
    const auto& collider = ecs.GetComponent<Collider>(entity);
    const auto& transform = ecs.GetComponent<Transform>(entity);
    const auto* rigid_body = ecs.TryGetComponent<RigidBody>(entity);
    const bool dynamic = rigid_body != nullptr;

    const BodyHandle body = world.CreateBody(
        BodyDesc{.shape = collider.shape,
                 .pose = {.position = transform.position,
                          .rotation = transform.rotation},
                 .scale = transform.scale,
                 .motion = dynamic ? Motion::kDynamic : Motion::kStatic,
                 .material = collider.material,
                 .velocity = dynamic ? rigid_body->velocity : bx::Vec3{0.0F},
                 .gravity = dynamic && rigid_body->has_gravity,
                 .user_data = Tag(entity)});
    if (!body.IsValid()) {
      continue;
    }
    if (dynamic) {
      world.SetAcceleration(body, rigid_body->acceleration);
    }
    ecs.AddComponent(entity, PhysicsLink(body, transform, collider,
                                         dynamic ? std::optional(*rigid_body)
                                                 : std::nullopt));
  }
}

void PhysicsSystem::PushMotion(PhysicsWorld& world, PhysicsLink& link,
                               const RigidBody* rigid_body) {
  const bool dynamic = rigid_body != nullptr;
  if (dynamic != link.rigid_body_.has_value()) {
    world.SetMotion(link.body_, dynamic ? Motion::kDynamic : Motion::kStatic);
    if (dynamic) {
      // Static bodies hold no gravity, velocity or acceleration
      world.SetGravityEnabled(link.body_, rigid_body->has_gravity);
      world.SetVelocity(link.body_, rigid_body->velocity);
      world.SetAcceleration(link.body_, rigid_body->acceleration);
    }
  } else if (dynamic) {
    const RigidBody& synced = *link.rigid_body_;
    if (!Same(rigid_body->velocity, synced.velocity)) {
      world.SetVelocity(link.body_, rigid_body->velocity);
    }
    if (rigid_body->has_gravity != synced.has_gravity) {
      world.SetGravityEnabled(link.body_, rigid_body->has_gravity);
    }
    if (!Same(rigid_body->acceleration, synced.acceleration)) {
      world.SetAcceleration(link.body_, rigid_body->acceleration);
    }
  }
  link.rigid_body_ =
      dynamic ? std::optional(*rigid_body) : std::optional<RigidBody>{};
}

void PhysicsSystem::PushEdits(Ecs& ecs, PhysicsWorld& world) {
  ecs.View<PhysicsLink, Collider, Transform>().ForEach(
      [&](Entity entity, PhysicsLink& link, const Collider& collider,
          const Transform& transform) -> void {
        if (!Same(transform.position, link.transform_.position) ||
            !Same(transform.rotation, link.transform_.rotation)) {
          world.SetPose(link.body_, {.position = transform.position,
                                     .rotation = transform.rotation});
        }
        if (!Same(collider.shape, link.collider_.shape) ||
            !Same(transform.scale, link.transform_.scale)) {
          world.SetShape(link.body_, collider.shape, transform.scale);
        }
        if (!Same(collider.material, link.collider_.material)) {
          world.SetMaterial(link.body_, collider.material);
        }
        PushMotion(world, link, ecs.TryGetComponent<RigidBody>(entity));
        link.transform_ = transform;
        link.collider_ = collider;
      });
}

void PhysicsSystem::PullResults(Ecs& ecs, const PhysicsWorld& world) {
  ecs.View<PhysicsLink, Transform>().ForEach(
      [&](Entity entity, PhysicsLink& link, Transform& transform) -> void {
        auto* rigid_body = ecs.TryGetComponent<RigidBody>(entity);
        if (!link.rigid_body_.has_value() || rigid_body == nullptr) {
          return;
        }
        const Pose pose = world.GetPose(link.body_);
        transform.position = pose.position;
        transform.rotation = pose.rotation;
        rigid_body->velocity = world.GetVelocity(link.body_);

        link.transform_ = transform;
        link.rigid_body_->velocity = rigid_body->velocity;
      });
}

void PhysicsSystem::Update(Ecs& ecs, PhysicsWorld& world,
                           const FrameContext& ctx) {
  SweepOrphans(ecs, world);
  DetachBodies(ecs, world);
  AttachBodies(ecs, world);
  PushEdits(ecs, world);

  // A long hitch would otherwise queue more steps than the frame can afford.
  accumulator_ += std::min(ctx.dt, kMaxFrameDt);

  // Drain whole steps only; the remainder carries to the next frame.
  while (accumulator_ >= kFixedDt) {
    world.Step(kFixedDt);
    accumulator_ -= kFixedDt;
  }

  PullResults(ecs, world);
}

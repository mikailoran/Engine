#include "ecs/systems/physics_system.h"

#include <bx/math.h>

#include <algorithm>
#include <cstdlib>

#include "ecs/components/rigid_body.h"
#include "ecs/components/spin.h"
#include "ecs/components/transform.h"
#include "ecs/core/ecs.h"
#include "ecs/core/frame_context.h"
#include "ecs/core/types.h"

void Physics::Init() {}

void Physics::Step(Ecs& ecs, const float fixed_dt) {
  ecs.View<Transform, RigidBody, Spin>().ForEach(
      [fixed_dt](Entity, Transform& transform, RigidBody& rigid_body,
                 const Spin& spin) {
        bx::Vec3 acceleration = rigid_body.acceleration;
        if (rigid_body.has_gravity) {
          acceleration = bx::add(acceleration, {0.0F, kGravity, 0.0F});
        }

        rigid_body.velocity =
            bx::add(rigid_body.velocity, bx::mul(acceleration, fixed_dt));

        transform.position =
            bx::add(transform.position, bx::mul(rigid_body.velocity, fixed_dt));

        if (transform.position.y < 0.0F) {
          transform.position.y = 0.0F;
          // Rest threshold stops entities from jittering on an almost full stop
          rigid_body.velocity.y =
              std::abs(rigid_body.velocity.y) < kRestThreshold
                  ? 0.0F
                  : rigid_body.velocity.y * -kRestitutionFactor;
        }

        if (spin.should_spin) {
          transform.rotation.y += spin.radians_per_second * fixed_dt;
        }
      });
}

void Physics::Update(Ecs& ecs, const FrameContext& ctx) {
  // A long hitch would otherwise queue more steps than the frame can afford.
  accumulator_ += std::min(ctx.dt, kMaxFrameDt);

  // Drain whole steps only; the remainder carries to the next frame.
  while (accumulator_ >= kFixedDt) {
    Step(ecs, kFixedDt);
    accumulator_ -= kFixedDt;
  }
}

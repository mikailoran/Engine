#include "physics_system.h"

#include "../components/spin.h"
#include "../components/transform.h"
#include "../core/ecs.h"
#include "../core/frame_context.h"

#include <algorithm>

void Physics::Init() {}

void Physics::Step(Ecs &ecs, const float fixed_dt) {
  for (const auto &entity : entities) {
    auto &transform = ecs.GetComponent<Transform>(entity);
    const auto &spin = ecs.GetComponent<Spin>(entity);

    if (spin.should_spin) {
      transform.rotation.y += spin.radians_per_second * fixed_dt;
    }
  }
}

void Physics::Update(Ecs &ecs, const FrameContext &ctx) {
  // A long hitch would otherwise queue more steps than the frame can afford.
  accumulator_ += std::min(ctx.dt, kMaxFrameDt);

  // Drain whole steps only; the remainder carries to the next frame.
  while (accumulator_ >= kFixedDt) {
    Step(ecs, kFixedDt);
    accumulator_ -= kFixedDt;
  }
}

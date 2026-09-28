#include "physics_system.h"

#include "../components/spin.h"
#include "../components/transform.h"
#include "../core/ecs.h"
#include "../core/frame_context.h"

void Physics::Init() {}

void Physics::Update(Ecs &ecs, const FrameContext &ctx) {
  for (const auto &entity : entities) {
    auto &transform = ecs.GetComponent<Transform>(entity);
    const auto &spin = ecs.GetComponent<Spin>(entity);

    if (spin.should_spin) {
      transform.rotation.y += spin.radians_per_second * ctx.dt;
    }
  }
}

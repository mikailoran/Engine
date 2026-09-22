#include "physics_system.h"

#include "../components/spin.h"
#include "../components/transform.h"
#include "../core/ecs.h"

void Physics::Init() {}

void Physics::Update(Ecs &ecs, float dt) {
  for (const auto &entity : entities) {
    auto &transform = ecs.GetComponent<Transform>(entity);
    const auto &spin = ecs.GetComponent<Spin>(entity);

    transform.rotation.y += spin.radians_per_second * dt;
  }
}

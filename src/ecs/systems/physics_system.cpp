#include "physics_system.h"

#include "../components/transform.h"
#include "../core/ecs.h"

void Physics::Init() {}

void Physics::Update(Ecs &ecs, float dt) {
  for (const auto &entity : entities) {
    auto &transform = ecs.GetComponent<Transform>(entity);

    transform.rotation.y += 2.0f * dt;
  }
}

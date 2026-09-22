#pragma once

#include "../core/system.h"

class Ecs;

class Physics : public System {
public:
  void Init();

  // TODO: Switch from passing ECS directly, to Views (apparently a lot of work)
  void Update(Ecs &ecs, float dt);

private:
};
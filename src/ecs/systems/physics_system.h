#pragma once

#include "../core/ecs.h"
#include "../core/system.h"

class Physics : public System {
public:
  void Init();

  void Update(Ecs &ecs, float dt);

private:
};
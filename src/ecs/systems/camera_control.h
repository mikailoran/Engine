#pragma once

#include "../core/system.h"

class CameraControl : public System {
public:
  void Init();

  void Update(float dt);

private:
};
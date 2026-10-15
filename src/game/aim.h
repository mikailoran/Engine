#pragma once

#include <bx/bounds.h>

namespace game {

struct Aim {
  bx::Ray ray{};
  bool trigger{false};
};

}  // namespace game

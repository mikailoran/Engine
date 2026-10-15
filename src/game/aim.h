#pragma once

#include <bx/bounds.h>

struct Aim {
  bx::Ray ray{};
  bool trigger{false};
};
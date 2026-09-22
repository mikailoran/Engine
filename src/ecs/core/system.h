#pragma once

#include "types.h"
#include <set>

struct System {
  // TODO: figure it out bro
  virtual ~System() = default;
  std::set<Entity> entities;
  Signature signature;
};
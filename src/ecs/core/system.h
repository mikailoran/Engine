#pragma once

#include "types.h"
#include <set>

struct System {
  // TODO: Make System virtual
  virtual ~System() = default;
  System() = default;
  System(const System &) = delete;
  System &operator=(const System &) = delete;

  std::set<Entity> entities;
  Signature signature;
};
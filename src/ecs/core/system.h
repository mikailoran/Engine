#pragma once

#include "types.h"
#include <set>

struct System {
  // TODO: Make System virtual
  // TODO: Reinforce invariants. Make better API by making member vars private
  virtual ~System() = default;
  System() = default;
  System(const System &) = delete;
  System &operator=(const System &) = delete;

  std::set<Entity> entities;
  Signature signature;
};
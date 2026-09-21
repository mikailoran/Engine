#pragma once

#include "types.h"
#include <set>

struct System {
  std::set<Entity> entities;
  Signature signature;
};
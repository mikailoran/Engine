#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>

using EntityType = std::size_t;
using Entity = std::size_t;
constexpr const EntityType MAX_ENTITIES = 5000;

using ComponentType = std::uint8_t;
constexpr const ComponentType MAX_COMPONENTS = 32;

using Signature = std::bitset<MAX_COMPONENTS>;

class TypeId {
public:
  template <class T> static ComponentType Get() {
    static const ComponentType id = next_++;
    return id;
  }

private:
  static inline ComponentType next_ = 0;
};

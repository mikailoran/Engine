#pragma once

#include <bitset>
#include <cstddef>
// #include <cstdint>

using EntityType = std::size_t;
using Entity = std::size_t;
constexpr const EntityType MAX_ENTITIES = 5000;

using ComponentType = std::size_t;
constexpr const ComponentType MAX_COMPONENTS = 32;

using Signature = std::bitset<MAX_COMPONENTS>;

class TypeId {
public:
  template <class T> static std::size_t Get() {
    static const std::size_t id = next_++;
    return id;
  }

private:
  static inline std::size_t next_ = 0;
};

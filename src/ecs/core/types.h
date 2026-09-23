#pragma once

#include <bitset>
#include <cstddef>

// TODO: Figure out Entity vs EntityType
using EntityType = std::size_t;
using Entity = std::size_t;
constexpr const EntityType MAX_ENTITIES = 5000;

using ComponentType = std::size_t;
constexpr const ComponentType MAX_COMPONENTS = 32;

using Signature = std::bitset<MAX_COMPONENTS>;

// TODO: Create a persistable ID system
/**
 * @brief Hands out a unique, monotonically increasing id per type @p T.
 *
 * One counter exists per @p Domain, so ids from different domains are
 * independent sequences both starting at zero.
 *
 * Ids are assigned on first use and stable for the life of the program, but
 * they depend on first-use order at runtime, so they must not be persisted or
 * compared across builds.
 *
 * @tparam Domain Tag type selecting which counter to draw from.
 */
template <class Domain> class TypeIdGen {
public:
  /**
   * @brief Returns the id for @p T within this domain.
   * @tparam T Type to identify.
   * @return Stable id, assigned on first call for @p T.
   */
  template <class T> static std::size_t Get() {
    static const std::size_t id = next_++;
    return id;
  }

private:
  static inline std::size_t next_ = 0;
};

// Tag selecting the component id sequence; these ids index Signature.
struct ComponentDomain {};
// Tag selecting the system id sequence; these ids are only map keys.
struct SystemDomain {};

using ComponentTypeId = TypeIdGen<ComponentDomain>;
using SystemTypeId = TypeIdGen<SystemDomain>;

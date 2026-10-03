#pragma once

#include <array>
#include <queue>

#include "types.h"

/**
 * @brief Hands out entity ids and tracks which of them are in use.
 *
 * The id space is fixed at MAX_ENTITIES and pre-seeded into a FIFO free pool
 * at construction, so an id is an index into every per-entity array here and
 * never needs allocating. A destroyed id goes to the back of the pool, which
 * delays reuse but does not prevent it: an Entity is a bare index with no
 * generation tag, so a handle held past its entity's destruction will silently
 * refer to whatever is recycled into that slot. Callers that cache an Entity
 * across frames must re-check IsAlive.
 */
class EntityManager {
 public:
  /** @brief Seeds the free pool with every id in [0, MAX_ENTITIES). */
  EntityManager();

  /**
   * @brief Takes the next free id and marks it alive.
   *
   * @return An id not currently in use, < MAX_ENTITIES.
   * @pre Fewer than MAX_ENTITIES entities are alive.
   */
  Entity CreateEntity();

  /**
   * @brief Tests whether an id is currently in use.
   *
   * @param entity Entity to test. Must be < MAX_ENTITIES.
   * @return True if the id has been created and not yet destroyed.
   */
  [[nodiscard]] bool IsAlive(Entity entity) const;

  /**
   * @brief Returns the entity's id to the free pool.
   *
   * Idempotent: destroying an id that is not alive does nothing, so the id
   * cannot be pushed onto the pool twice and handed out to two live entities.
   *
   * Destroys no components. Ecs::Flush is what sequences that alongside this
   * call.
   *
   * @param entity Entity to destroy. Must be < MAX_ENTITIES; need not be
   *               alive.
   */
  void DestroyEntity(Entity entity);

 private:
  std::queue<Entity> available_entities_;
  EntityType living_entity_count_{0};
  std::array<bool, kMaxEntities> alive_{};
};
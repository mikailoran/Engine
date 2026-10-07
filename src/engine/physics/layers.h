#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>

#include <optional>

namespace engine::physics {

/** @brief Jolt object layers, one per kind of body. */
namespace object_layer {
inline constexpr JPH::ObjectLayer kNonMoving = 0;
inline constexpr JPH::ObjectLayer kMoving = 1;
inline constexpr JPH::uint kCount = 2;
}  // namespace object_layer

/**
 * @brief Jolt's collision filtering: moving bodies collide with everything,
 * non-moving bodies only with moving ones.
 *
 * The PhysicsSystem keeps references to these tables, so this must outlive it.
 */
class CollisionLayers {
 public:
  /** @brief Builds and fills the layer tables. */
  CollisionLayers();

  /** @brief Maps each object layer to its broad-phase layer. */
  [[nodiscard]] auto BroadPhase() const -> const JPH::BroadPhaseLayerInterface&;

  /** @brief Filters object layers against broad-phase layers. */
  [[nodiscard]] auto ObjectVsBroadPhase() const
      -> const JPH::ObjectVsBroadPhaseLayerFilter&;

  /** @brief Filters pairs of object layers. */
  [[nodiscard]] auto ObjectPairs() const -> const JPH::ObjectLayerPairFilter&;

 private:
  JPH::BroadPhaseLayerInterfaceTable broad_phase_;
  JPH::ObjectLayerPairFilterTable object_pairs_;
  // Derived from the two tables above, so built once they are filled
  std::optional<JPH::ObjectVsBroadPhaseLayerFilterTable> object_vs_broad_phase_;
};

}  // namespace engine::physics

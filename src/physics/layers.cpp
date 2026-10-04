#include "physics/layers.h"

#include <Jolt/Core/Core.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

#include <cassert>

namespace {

// One broad-phase layer per object layer
constexpr JPH::BroadPhaseLayer kNonMovingBroadPhase{0};
constexpr JPH::BroadPhaseLayer kMovingBroadPhase{1};
constexpr JPH::uint kBroadPhaseCount = 2;

}  // namespace

CollisionLayers::CollisionLayers()
    : broad_phase_(object_layer::kCount, kBroadPhaseCount),
      object_pairs_(object_layer::kCount) {
  broad_phase_.MapObjectToBroadPhaseLayer(object_layer::kNonMoving,
                                          kNonMovingBroadPhase);
  broad_phase_.MapObjectToBroadPhaseLayer(object_layer::kMoving,
                                          kMovingBroadPhase);

  // Non-moving pairs stay disabled: static bodies never touch each other
  object_pairs_.EnableCollision(object_layer::kMoving, object_layer::kMoving);
  object_pairs_.EnableCollision(object_layer::kMoving,
                                object_layer::kNonMoving);

  object_vs_broad_phase_.emplace(broad_phase_, kBroadPhaseCount, object_pairs_,
                                 object_layer::kCount);
}

auto CollisionLayers::BroadPhase() const
    -> const JPH::BroadPhaseLayerInterface& {
  return broad_phase_;
}

auto CollisionLayers::ObjectVsBroadPhase() const
    -> const JPH::ObjectVsBroadPhaseLayerFilter& {
  assert(object_vs_broad_phase_.has_value() && "built in the constructor");
  return object_vs_broad_phase_.value();
}

auto CollisionLayers::ObjectPairs() const -> const JPH::ObjectLayerPairFilter& {
  return object_pairs_;
}

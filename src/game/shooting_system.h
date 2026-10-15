#pragma once

namespace engine {
class Engine;
class Ecs;
}  // namespace engine

namespace game {

auto HandleShots(engine::Engine& engine) -> void;

auto UpdateCooldowns(engine::Ecs& ecs, float dt) -> void;

}  // namespace game

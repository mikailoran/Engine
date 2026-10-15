#pragma once

namespace engine {
class Engine;
class Ecs;
struct FrameContext;
struct YawPitch;
}  // namespace engine

namespace game {
auto HandleShots(engine::Engine& engine, const engine::FrameContext& ctx)
    -> void;

auto Shoot(engine::Engine& engine) -> void;

auto UpdateCooldowns(engine::Engine& engine, const engine::FrameContext& ctx)
    -> void;

}  // namespace game
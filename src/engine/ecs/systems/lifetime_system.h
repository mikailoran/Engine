#pragma once

namespace engine {
class Ecs;
struct FrameContext;

/**
 * @brief Counts every Lifetime down by the frame's dt and queues expired
 * entities for destruction. Run in Engine::Update; the next Ecs::Flush
 * destroys them.
 */
auto ExpireLifetimes(Ecs& ecs, const FrameContext& ctx) -> void;

}  // namespace engine
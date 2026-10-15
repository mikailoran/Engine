#pragma once

namespace engine {
class Ecs;
struct FrameContext;

auto ExpireLifetimes(Ecs& ecs, const FrameContext& ctx) -> void;

}  // namespace engine
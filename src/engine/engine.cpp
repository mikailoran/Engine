#include "engine/engine.h"

#include <bx/bounds.h>

#include <filesystem>
#include <optional>
#include <string>
#include <utility>

#include "engine/ecs/components/animator.h"
#include "engine/ecs/components/attached_meshes.h"
#include "engine/ecs/components/camera.h"
#include "engine/ecs/components/character_body.h"
#include "engine/ecs/components/character_link.h"
#include "engine/ecs/components/collider.h"
#include "engine/ecs/components/directional_light.h"
#include "engine/ecs/components/lifetime.h"
#include "engine/ecs/components/name.h"
#include "engine/ecs/components/physics_link.h"
#include "engine/ecs/components/ragdoll.h"
#include "engine/ecs/components/ragdoll_link.h"
#include "engine/ecs/components/renderable.h"
#include "engine/ecs/components/rigid_body.h"
#include "engine/ecs/components/skin_pose.h"
#include "engine/ecs/components/sky.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/ecs/systems/lifetime_system.h"
#include "engine/ecs/systems/physics_query.h"
#include "engine/ecs/systems/physics_system.h"
#include "engine/hud/crosshair.h"
#include "engine/platform/frame_context.h"
#include "engine/platform/native_surface.h"
#include "engine/platform/screen.h"
#include "engine/resource/asset_registry.h"
#include "engine/scene/builtin_loaders.h"
#include "engine/scene/picking.h"
#include "engine/scene/scene_loader.h"

namespace engine {

Engine::Engine(const NativeSurface& surface) : bgfx_context_(surface) {
  ecs_.RegisterComponent<Name>();
  ecs_.RegisterComponent<Camera>();
  ecs_.RegisterComponent<Renderable>();
  ecs_.RegisterComponent<DirectionalLight>();
  ecs_.RegisterComponent<Sky>();
  ecs_.RegisterComponent<Transform>();
  ecs_.RegisterComponent<RigidBody>();
  ecs_.RegisterComponent<Collider>();
  ecs_.RegisterComponent<PhysicsLink>();
  ecs_.RegisterComponent<CharacterBody>();
  ecs_.RegisterComponent<CharacterLink>();
  ecs_.RegisterComponent<Lifetime>();
  ecs_.RegisterComponent<Animator>();
  ecs_.RegisterComponent<SkinPose>();
  ecs_.RegisterComponent<AttachedMeshes>();
  ecs_.RegisterComponent<Ragdoll>();
  ecs_.RegisterComponent<RagdollLink>();

  RegisterBuiltinLoaders(scene_loader_);
}

void Engine::Resize(PixelSize size) { bgfx_context_.Resize(size); }

void Engine::Update(const FrameContext& ctx) {
  physics_system_.Update(ecs_, physics_world_, ctx);
  // After physics: gameplay picks clips from this frame's motion
  animation_.Update(ecs_, assets_, ctx);
  // Kinematic ragdolls take this frame's pose, for ray casts after Update
  PhysicsSystem::PoseRagdolls(ecs_, physics_world_, ctx);
  ExpireLifetimes(ecs_, ctx);
  // Frame uniforms must be set before the renderer submits
  lighting_.Update(ecs_, ctx);
}

void Engine::Render(const FrameContext& ctx, RenderOptions options) {
  // Submits to kShadowView; must precede the renderer's bgfx::frame()
  shadow_.Update(ecs_, assets_, ctx);
  // Submits to kSceneView; must precede the renderer's bgfx::frame()
  sky_.Update(ecs_);
  // Submits to kDebugView; must precede the renderer's bgfx::frame()
  if (options.debug_draw) {
    debug_draw_.Update(ecs_, assets_, physics_world_, ctx);
    bgfx_context_.SetDebugStats(true);
  } else {
    bgfx_context_.SetDebugStats(false);
  }
  // Submits to kHudView; must precede the renderer's bgfx::frame()
  hud_.Update(ctx, options.hud);
  render_.Update(ecs_, assets_, ctx, shadow_.Map());
}

void Engine::EndFrame() { ecs_.Flush(); }

void Engine::LoadScene(const std::filesystem::path& path) {
  scene_loader_.Load(path, ecs_, assets_, physics_world_);
}

auto Engine::SpawnPrefab(const std::filesystem::path& prefab,
                         const Transform& pose) -> Entity {
  const auto entity =
      scene_loader_.Spawn(prefab, ecs_, assets_, physics_world_);
  if (ecs_.HasComponent<Transform>(entity)) {
    ecs_.GetComponent<Transform>(entity) = pose;
  } else {
    ecs_.AddComponent(entity, pose);
  }
  return entity;
}

void Engine::ClearWorld() {
  // Dead ids are ignored, so every possible id can be queued
  for (Entity entity = 0; entity < kMaxEntities; ++entity) {
    ecs_.DestroyEntity(entity);
  }
  ecs_.Flush();
}

void Engine::RegisterSceneLoader(std::string key, ComponentLoader loader) {
  scene_loader_.Register(std::move(key), std::move(loader));
}

void Engine::SetActiveCamera(Entity camera) {
  active_camera_ = camera;
  render_.SetActiveCamera(camera);
  shadow_.SetActiveCamera(camera);
  debug_draw_.SetActiveCamera(camera);
}

auto Engine::Pick(ScreenPosition position, ScreenSize size)
    -> std::optional<Entity> {
  if (!active_camera_) {
    return std::nullopt;
  }
  return engine::Pick(ecs_, assets_, *active_camera_, position, size);
}

auto Engine::CastRay(const bx::Ray& ray, float max_distance,
                     std::optional<Entity> ignore) -> std::optional<RayHit> {
  return engine::CastRay(ecs_, physics_world_, ray, max_distance, ignore);
}

void Engine::Highlight(Entity entity) { debug_draw_.Highlight(entity); }

void Engine::DrawCrosshair(const CrosshairStyle& style) {
  hud_.DrawCrosshair(style);
}

auto Engine::World() -> Ecs& { return ecs_; }

auto Engine::Assets() -> AssetRegistry& { return assets_; }

}  // namespace engine

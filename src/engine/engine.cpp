#include "engine/engine.h"

#include <filesystem>
#include <optional>

#include "engine/ecs/components/camera.h"
#include "engine/ecs/components/collider.h"
#include "engine/ecs/components/directional_light.h"
#include "engine/ecs/components/physics_link.h"
#include "engine/ecs/components/renderable.h"
#include "engine/ecs/components/rigid_body.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/platform/frame_context.h"
#include "engine/platform/native_surface.h"
#include "engine/platform/screen.h"
#include "engine/resource/asset_registry.h"
#include "engine/scene/builtin_loaders.h"
#include "engine/scene/picking.h"
#include "engine/scene/scene_loader.h"

Engine::Engine(const NativeSurface& surface) : bgfx_context_(surface) {
  ecs_.RegisterComponent<Camera>();
  ecs_.RegisterComponent<Renderable>();
  ecs_.RegisterComponent<DirectionalLight>();
  ecs_.RegisterComponent<Transform>();
  ecs_.RegisterComponent<RigidBody>();
  ecs_.RegisterComponent<Collider>();
  ecs_.RegisterComponent<PhysicsLink>();

  RegisterBuiltinLoaders(scene_loader_);
}

void Engine::Resize(PixelSize size) { bgfx_context_.Resize(size); }

void Engine::Update(const FrameContext& ctx) {
  physics_system_.Update(ecs_, physics_world_, ctx);
  // Frame uniforms must be set before the renderer submits
  lighting_.Update(ecs_, ctx);
}

void Engine::Render(const FrameContext& ctx, RenderOptions options) {
  // Submits to view 1; must precede the renderer's bgfx::frame()
  if (options.debug_draw) {
    debug_draw_.Update(ecs_, assets_, ctx);
  }
  render_.Update(ecs_, assets_, ctx);
}

void Engine::EndFrame() { ecs_.Flush(); }

void Engine::LoadScene(const std::filesystem::path& path) {
  scene_loader_.Load(path, ecs_, assets_);
}

void Engine::SetActiveCamera(Entity camera) {
  active_camera_ = camera;
  render_.SetActiveCamera(camera);
  debug_draw_.SetActiveCamera(camera);
}

auto Engine::Pick(ScreenPosition position, ScreenSize size)
    -> std::optional<Entity> {
  if (!active_camera_) {
    return std::nullopt;
  }
  return ::Pick(ecs_, assets_, *active_camera_, position, size);
}

void Engine::Highlight(Entity entity) { debug_draw_.Highlight(entity); }

auto Engine::World() -> Ecs& { return ecs_; }

auto Engine::Assets() -> AssetRegistry& { return assets_; }

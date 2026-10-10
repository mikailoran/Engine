#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "engine/bgfx_context.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/ecs/systems/debug_draw_system.h"
#include "engine/ecs/systems/lighting_system.h"
#include "engine/ecs/systems/physics_system.h"
#include "engine/ecs/systems/render_system.h"
#include "engine/physics/jolt_runtime.h"
#include "engine/physics/physics_world.h"
#include "engine/platform/native_surface.h"
#include "engine/platform/screen.h"
#include "engine/resource/asset_registry.h"
#include "engine/scene/scene_loader.h"

namespace engine {

struct FrameContext;

/** @brief What Render draws this frame beyond the scene. */
struct RenderOptions {
  // Grid, axes, collider wireframes, velocity arrows and highlights.
  bool debug_draw{true};
};

/**
 * @brief Rendering, physics, assets and the ECS world, driven by a host.
 *
 * The host owns the window, the event loop and the clock. Each frame it calls
 * Update, Render and EndFrame in that order, running its own tools between
 * them. Member order is the bring-up order and teardown runs in reverse, so
 * bgfx outlives every GPU resource. The asset root must be set first.
 */
class Engine {
 public:
  /**
   * @brief Brings up bgfx against @p surface, then physics, assets and the
   * world, with the engine's components registered.
   * @throws std::runtime_error If bgfx or a system's resources fail to load.
   */
  explicit Engine(const NativeSurface& surface);

  /**
   * @brief Matches the backbuffer to @p size, e.g. after the window or its
   * scale changes. Cheap when unchanged, so hosts can call it every frame.
   */
  void Resize(PixelSize size);

  /** @brief Steps physics and sets this frame's lighting. */
  void Update(const FrameContext& ctx);

  /** @brief Draws the frame and submits it with bgfx::frame. After Update. */
  void Render(const FrameContext& ctx, RenderOptions options);

  /** @brief Destroys the entities whose destruction was requested. */
  void EndFrame();

  /**
   * @brief Loads a scene file's entities into the world.
   * @param path Relative to the asset root, e.g. "assets/scenes/debug.json".
   * @throws std::runtime_error If the scene or an asset it names won't load.
   */
  void LoadScene(const std::filesystem::path& path);

  /**
   * @brief Registers how scenes load a host's own component, under @p key.
   * @pre @p key is not registered yet.
   */
  void RegisterSceneLoader(std::string key, ComponentLoader loader);

  /**
   * @brief Nominates the camera rendered and picked from. It must carry a
   * Transform and a Camera.
   */
  void SetActiveCamera(Entity camera);

  /**
   * @brief Finds the entity under @p position, seen from the active camera.
   * @param size Backbuffer size; must have positive area.
   * @return Empty on a miss, or when no camera is active.
   */
  auto Pick(ScreenPosition position, ScreenSize size) -> std::optional<Entity>;

  /**
   * @brief Requests a wire box around @p entity for this frame. Only a Render
   * with debug draw on draws and clears requests, so request only then.
   */
  void Highlight(Entity entity);

  /** @brief The ECS world, for hosts and their tools to read and edit. */
  auto World() -> Ecs&;

  /** @brief The registry every mesh is loaded through. */
  auto Assets() -> AssetRegistry&;

 private:
  // Before anything that creates GPU resources
  BgfxContext bgfx_context_;
  // Before anything that uses Jolt
  physics::JoltRuntime jolt_runtime_;

  AssetRegistry assets_;
  physics::PhysicsWorld physics_world_{
      jolt_runtime_, static_cast<std::uint32_t>(kMaxEntities)};
  Ecs ecs_;
  SceneLoader scene_loader_;

  PhysicsSystem physics_system_;
  LightingSystem lighting_;
  DebugDrawSystem debug_draw_;
  RenderSystem render_;

  // Camera rendered and picked from; empty until SetActiveCamera.
  std::optional<Entity> active_camera_;
};

}  // namespace engine

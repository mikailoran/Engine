#pragma once

class Ecs;
class AssetRegistry;
struct FrameContext;
struct ImGuiContext;

/**
 * @brief Draws the debug settings window and applies its edits.
 *
 * Owns the ImGui context, a process-wide global, so at most one instance may
 * exist. Requires bgfx::init to have completed.
 */
class UiSystem {
 public:
  /** @brief Creates the ImGui context and keeps a handle to it. */
  UiSystem();

  /** @brief Destroys the ImGui context. Must run before bgfx::shutdown. */
  ~UiSystem();

  // Owns a global: copying or moving would destroy it twice
  UiSystem(const UiSystem&) = delete;
  auto operator=(const UiSystem&) -> UiSystem& = delete;
  UiSystem(UiSystem&&) = delete;
  auto operator=(UiSystem&&) -> UiSystem& = delete;

  /**
   * @brief Builds this frame's UI and applies its edits.
   *
   * @param ecs World to edit and spawn entities into.
   * @param assets Registry to load spawned entities' meshes through.
   * @param ctx Per-frame inputs.
   */
  void Update(Ecs& ecs, AssetRegistry& assets, const FrameContext& ctx);

 private:
  // Created by imguiCreate and freed by imguiDestroy; never null.
  ImGuiContext* context_{nullptr};
};

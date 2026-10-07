#pragma once

class Engine;
struct FrameContext;

/**
 * @brief Selects the entity under the cursor on a fresh left click.
 *
 * Picks through the engine's active camera (see Engine::Pick). The entity
 * hit gets Selected; a miss clears the selection. Single selection only.
 * Reads the camera's pose, so must run after FlyCameraSystem.
 *
 * @param engine Engine to pick through and write Selected into.
 * @param ctx Per-frame inputs; reads the mouse and window size.
 * @param mouse_over_ui True when the UI owns the cursor; clicks are ignored.
 */
void UpdateSelection(Engine& engine, const FrameContext& ctx,
                     bool mouse_over_ui);

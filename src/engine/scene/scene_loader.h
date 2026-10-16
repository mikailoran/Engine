#pragma once

#include <filesystem>
#include <functional>
#include <nlohmann/json.hpp>  // IWYU pragma: keep (prefabs_ holds values)
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <unordered_map>

#include "engine/ecs/core/types.h"

namespace engine {

class Ecs;
class AssetRegistry;

namespace physics {
class PhysicsWorld;
}  // namespace physics

/**
 * @brief Reads one component's JSON block and adds it to @p entity, loading
 * any assets it names through the registry; collision meshes are built in
 * @p physics.
 */
using ComponentLoader =
    std::function<void(const nlohmann::json& data, Entity entity, Ecs& ecs,
                       AssetRegistry& assets, physics::PhysicsWorld& physics)>;

/**
 * @brief Loads JSON scene files into the ECS through per-component loaders.
 *
 * Each entity's "components" object maps a registered key to that
 * component's data. An entity may also name a "prefab" file (relative to the
 * asset root) whose "components" it starts from; its own "components" are
 * merged over them as a JSON merge patch (RFC 7386), so an override can set
 * single fields and null removes a component. Each prefab file is read once
 * per loader and cached, so edits to it on disk need a restart.
 */
class SceneLoader {
 public:
  /** @brief Registers the loader for a component key. @pre Key is unused. */
  void Register(std::string key, ComponentLoader loader);

  /**
   * @brief Creates every entity described in a scene file.
   *
   * @param path Scene file, relative to the asset root.
   * @param ecs World to create the entities in.
   * @param assets Registry to load the scene's meshes through.
   * @param physics World to build the scene's collision meshes in.
   * @throws std::runtime_error On an unreadable scene or prefab file,
   *         malformed JSON, a wrong version, an unknown key or a missing
   *         field.
   */
  void Load(const std::filesystem::path& path, Ecs& ecs, AssetRegistry& assets,
            physics::PhysicsWorld& physics);

  /**
   * @brief Creates one entity from a prefab file, as a scene entry naming
   * only that prefab would.
   *
   * @param prefab Prefab file, relative to the asset root.
   * @return The new entity, named after the file's stem ("enemy").
   * @throws std::runtime_error On an unreadable or invalid prefab, an unknown
   *         component key or an asset that won't load.
   */
  auto Spawn(const std::filesystem::path& prefab, Ecs& ecs,
             AssetRegistry& assets, physics::PhysicsWorld& physics) -> Entity;

 private:
  /** @brief Validates a parsed scene and creates its entities. */
  void LoadEntities(const nlohmann::json& scene, Ecs& ecs,
                    AssetRegistry& assets, physics::PhysicsWorld& physics);

  /**
   * @brief Creates one named entity and runs each component's loader on it.
   *
   * @param components Merged component blocks (prefab and overrides), keyed
   *        by registered loader key.
   * @throws std::runtime_error On an unknown key or a loader's JSON error,
   *         tagged with the entity and component.
   */
  auto CreateEntity(const std::string& name, const nlohmann::json& components,
                    Ecs& ecs, AssetRegistry& assets,
                    physics::PhysicsWorld& physics) const -> Entity;

  std::unordered_map<std::string, ComponentLoader> loaders_;
  /// Prefab path -> its "components" object, filled on first use
  std::unordered_map<std::string, nlohmann::json> prefabs_;
};

}  // namespace engine

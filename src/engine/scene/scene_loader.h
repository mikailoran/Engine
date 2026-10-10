#pragma once

#include <filesystem>
#include <functional>
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
 * component's data.
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
   * @throws std::runtime_error On an unreadable file, malformed JSON, a wrong
   *         version, an unknown key or a missing field.
   */
  void Load(const std::filesystem::path& path, Ecs& ecs, AssetRegistry& assets,
            physics::PhysicsWorld& physics) const;

 private:
  /** @brief Validates a parsed scene and creates its entities. */
  void LoadEntities(const nlohmann::json& scene, Ecs& ecs,
                    AssetRegistry& assets,
                    physics::PhysicsWorld& physics) const;

  std::unordered_map<std::string, ComponentLoader> loaders_;
};

}  // namespace engine

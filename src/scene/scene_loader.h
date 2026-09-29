#pragma once

#include "../ecs/core/types.h"

#include <filesystem>
#include <functional>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <unordered_map>

class Ecs;
class AssetRegistry;

/** @brief What a component loader may read from and write to. */
struct SceneLoadContext {
  Ecs &ecs;
  AssetRegistry &assets;
};

/** @brief Reads one component's JSON block and adds it to @p entity. */
using ComponentLoader = std::function<void(
    const nlohmann::json &data, Entity entity, SceneLoadContext &ctx)>;

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
   * @throws std::runtime_error On an unreadable file, malformed JSON, a wrong
   *         version, an unknown key or a missing field.
   */
  void Load(const std::filesystem::path &path, SceneLoadContext &ctx) const;

private:
  /** @brief Validates a parsed scene and creates its entities. */
  void LoadEntities(const nlohmann::json &scene, SceneLoadContext &ctx) const;

  std::unordered_map<std::string, ComponentLoader> loaders_;
};

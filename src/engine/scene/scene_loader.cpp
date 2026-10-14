#include "engine/scene/scene_loader.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include "engine/ecs/components/name.h"
#include "engine/ecs/core/ecs.h"
#include "engine/platform/asset_root.h"

namespace engine {

namespace {

// Scene format version this loader reads.
constexpr int kSceneVersion = 1;
// Prefab format version this loader reads.
constexpr int kPrefabVersion = 1;

// Prefab path -> its "components" object, parsed once per scene load
using PrefabCache = std::unordered_map<std::string, nlohmann::json>;

/**
 * @brief Opens and parses a JSON file.
 *
 * @throws std::runtime_error If the file can't be opened.
 * @throws nlohmann::json::exception If it isn't valid JSON.
 */
auto ReadJsonFile(const std::filesystem::path& full_path) -> nlohmann::json {
  std::ifstream file(full_path);
  if (!file) {
    throw std::runtime_error("cannot open " + full_path.string());
  }
  return nlohmann::json::parse(file);
}

/**
 * @brief Returns a prefab's "components" object, reading the file on first
 * use.
 *
 * @param path Prefab file, relative to the asset root.
 * @param cache Prefabs read so far; the result points into it.
 * @throws std::runtime_error On an unreadable file, malformed JSON, a wrong
 *         version or an unknown top-level key.
 */
auto PrefabComponents(const std::string& path, PrefabCache& cache)
    -> const nlohmann::json& {
  if (const auto it = cache.find(path); it != cache.end()) {
    return it->second;
  }

  try {
    auto prefab = ReadJsonFile(std::filesystem::path(AssetRoot()) / path);
    for (const auto& item : prefab.items()) {
      if (item.key() != "version" && item.key() != "components") {
        throw std::runtime_error("unknown top-level key '" + item.key() + "'");
      }
    }
    if (prefab.at("version").get<int>() != kPrefabVersion) {
      throw std::runtime_error("unsupported prefab version");
    }
    return cache.emplace(path, std::move(prefab.at("components")))
        .first->second;
  } catch (const nlohmann::json::exception& e) {
    throw std::runtime_error("prefab '" + path + "': " + e.what());
  } catch (const std::runtime_error& e) {
    throw std::runtime_error("prefab '" + path + "': " + e.what());
  }
}

}  // namespace

void SceneLoader::Register(std::string key, ComponentLoader loader) {
  [[maybe_unused]] const auto [it, inserted] =
      loaders_.emplace(std::move(key), std::move(loader));
  assert(inserted && "component loader registered twice");
}

void SceneLoader::LoadEntities(const nlohmann::json& scene, Ecs& ecs,
                               AssetRegistry& assets,
                               physics::PhysicsWorld& physics) const {
  // Reject keys this version does not know, so typos fail loudly
  for (const auto& item : scene.items()) {
    if (item.key() != "version" && item.key() != "entities") {
      throw std::runtime_error("unknown top-level key '" + item.key() + "'");
    }
  }

  if (scene.at("version").get<int>() != kSceneVersion) {
    throw std::runtime_error("unsupported scene version");
  }

  PrefabCache prefabs;

  // One entity per entry, then one loader call per component block
  for (const auto& entry : scene.at("entities")) {
    const auto name = entry.value("name", std::string{"<unnamed>"});
    // TODO: handle case where name is empty string

    // Prefab components first, then the entry's patched over them
    nlohmann::json components = nlohmann::json::object();
    try {
      if (entry.contains("prefab")) {
        components =
            PrefabComponents(entry.at("prefab").get<std::string>(), prefabs);
      }
      if (entry.contains("components")) {
        components.merge_patch(entry.at("components"));
      }
      if (!components.is_object()) {
        throw std::runtime_error("components must be an object");
      }
    } catch (const nlohmann::json::exception& e) {
      throw std::runtime_error("entity '" + name + "': " + e.what());
    } catch (const std::runtime_error& e) {
      throw std::runtime_error("entity '" + name + "': " + e.what());
    }

    const auto entity = ecs.CreateEntity();
    ecs.AddComponent(entity, Name{.value = name});

    for (const auto& component : components.items()) {
      const std::string where =
          "entity '" + name + "', component '" + component.key() + "': ";

      const auto it = loaders_.find(component.key());
      if (it == loaders_.end()) {
        throw std::runtime_error(where + "unknown component");
      }

      try {
        const auto loader = it->second;
        loader(component.value(), entity, ecs, assets, physics);
      } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(where + e.what());
      }
    }
  }
}

void SceneLoader::Load(const std::filesystem::path& path, Ecs& ecs,
                       AssetRegistry& assets,
                       physics::PhysicsWorld& physics) const {
  // Read the file relative to the asset root
  const auto full_path = std::filesystem::path(AssetRoot()) / path;

  // Tag every error with the file; JSON errors become runtime_error
  try {
    LoadEntities(ReadJsonFile(full_path), ecs, assets, physics);
  } catch (const nlohmann::json::exception& e) {
    throw std::runtime_error(full_path.string() + ": " + e.what());
  } catch (const std::runtime_error& e) {
    throw std::runtime_error(full_path.string() + ": " + e.what());
  }
}

}  // namespace engine

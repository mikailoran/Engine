#include "scene_loader.h"

#include "../ecs/core/ecs.h"
#include "../platform/asset_root.h"

#include <cassert>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace {

// Scene format version this loader reads.
constexpr int kSceneVersion = 1;

} // namespace

void SceneLoader::Register(std::string key, ComponentLoader loader) {
  [[maybe_unused]] const auto [it, inserted] =
      loaders_.emplace(std::move(key), std::move(loader));
  assert(inserted && "component loader registered twice");
}

void SceneLoader::LoadEntities(const nlohmann::json &scene,
                               SceneLoadContext &ctx) const {
  // Reject keys this version does not know, so typos fail loudly
  for (const auto &item : scene.items()) {
    if (item.key() != "version" && item.key() != "entities") {
      throw std::runtime_error("unknown top-level key '" + item.key() + "'");
    }
  }

  if (scene.at("version").get<int>() != kSceneVersion) {
    throw std::runtime_error("unsupported scene version");
  }

  // One entity per entry, then one loader call per component block
  for (const auto &entry : scene.at("entities")) {
    const auto name = entry.value("name", std::string{"<unnamed>"});
    const auto entity = ctx.ecs.CreateEntity();

    for (const auto &component : entry.at("components").items()) {
      const std::string where =
          "entity '" + name + "', component '" + component.key() + "': ";

      const auto it = loaders_.find(component.key());
      if (it == loaders_.end()) {
        throw std::runtime_error(where + "unknown component");
      }

      try {
        it->second(component.value(), entity, ctx);
      } catch (const nlohmann::json::exception &e) {
        throw std::runtime_error(where + e.what());
      }
    }
  }
}

void SceneLoader::Load(const std::filesystem::path &path,
                       SceneLoadContext &ctx) const {
  // Read the file relative to the asset root
  const auto full_path = std::filesystem::path(AssetRoot()) / path;
  std::ifstream file(full_path);
  if (!file) {
    throw std::runtime_error("cannot open scene file: " + full_path.string());
  }

  // Tag every error with the file; JSON errors become runtime_error
  try {
    LoadEntities(nlohmann::json::parse(file), ctx);
  } catch (const nlohmann::json::exception &e) {
    throw std::runtime_error(full_path.string() + ": " + e.what());
  } catch (const std::runtime_error &e) {
    throw std::runtime_error(full_path.string() + ": " + e.what());
  }
}

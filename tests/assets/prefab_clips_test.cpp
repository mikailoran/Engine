// Scene data the game can't check until it runs: every clip a prefab names
// must exist in the clip file its animator loads.

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

#include "engine/resource/animation_clip.h"
#include "engine/resource/gltf_reader.h"

namespace engine {

namespace {

/** @brief Every clip name an entity's components refer to. */
auto ClipNames(const nlohmann::json& components) -> std::vector<std::string> {
  std::vector<std::string> names;
  const auto& animator = components.at("animator");
  if (animator.contains("play")) {
    names.push_back(animator.at("play").get<std::string>());
  }
  if (components.contains("enemy")) {
    for (const auto& [role, name] :
         components.at("enemy").at("clips").items()) {
      names.push_back(name.get<std::string>());
    }
  }
  return names;
}

TEST(PrefabClips, EveryNamedClipExists) {
  const std::filesystem::path assets(ENGINE_ASSET_DIR);
  int animated = 0;
  for (const auto& entry :
       std::filesystem::directory_iterator(assets / "prefabs")) {
    SCOPED_TRACE(entry.path().string());
    std::ifstream file(entry.path());
    const auto components = nlohmann::json::parse(file).at("components");
    if (!components.contains("animator")) {
      continue;
    }
    ++animated;
    // Scene paths start at the asset root's parent, as "assets/..."
    const auto clips_path =
        components.at("animator").at("clips").get<std::string>();
    const ClipFile clips = ReadGltfClips(assets.parent_path() / clips_path);
    for (const std::string& name : ClipNames(components)) {
      EXPECT_TRUE(std::ranges::any_of(
          clips.clips,
          [&](const AnimationClip& clip) -> bool { return clip.name == name; }))
          << "no clip " << name << " in " << clips_path;
    }
  }
  EXPECT_GT(animated, 0) << "no animated prefab";
}

}  // namespace

}  // namespace engine

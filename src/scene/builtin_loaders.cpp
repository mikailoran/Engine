#include "scene/builtin_loaders.h"

#include <bx/math.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <stdexcept>
#include <string>

#include "ecs/components/collider.h"
#include "ecs/components/directional_light.h"
#include "ecs/components/renderable.h"
#include "ecs/components/transform.h"
#include "ecs/core/ecs.h"
#include "ecs/core/types.h"
#include "resource/asset_registry.h"
#include "scene/json_read.h"
#include "scene/scene_loader.h"

namespace {

using nlohmann::json;

/** @brief Adds a Transform; every field is optional. Rotation is in degrees. */
void LoadTransform(const json& data, Entity entity, Ecs& ecs,
                   AssetRegistry& /*assets*/) {
  CheckKeys(data, {"position", "rotation_deg", "scale"});

  Transform transform{};
  if (data.contains("position")) {
    transform.position = ReadVec3(data.at("position"));
  }
  if (data.contains("rotation_deg")) {
    const auto deg = ReadVec3(data.at("rotation_deg"));
    transform.rotation = {bx::toRad(deg.x), bx::toRad(deg.y), bx::toRad(deg.z)};
  }
  if (data.contains("scale")) {
    transform.scale = ReadVec3(data.at("scale"));
  }
  ecs.AddComponent(entity, transform);
}

/**
 * @brief Adds a Renderable; "mesh" is required, "color", "texture" and
 * "texture_scale" optional.
 */
void LoadRenderable(const json& data, Entity entity, Ecs& ecs,
                    AssetRegistry& assets) {
  CheckKeys(data, {"mesh", "color", "texture", "texture_scale"});

  Renderable renderable{};
  renderable.mesh_handle = assets.LoadMesh(data.at("mesh").get<std::string>());
  if (data.contains("color")) {
    renderable.color = ReadFloats<4>(data.at("color"));
  }
  if (data.contains("texture")) {
    renderable.texture =
        assets.LoadTexture(data.at("texture").get<std::string>());
  }
  if (data.contains("texture_scale")) {
    renderable.texture_scale = data.at("texture_scale").get<float>();
    // The shader divides by it
    if (renderable.texture_scale <= 0.0F) {
      throw std::runtime_error("texture_scale must be positive");
    }
  }
  ecs.AddComponent(entity, renderable);
}

/** @brief Adds a DirectionalLight; every field is optional. */
void LoadDirectionalLight(const json& data, Entity entity, Ecs& ecs,
                          AssetRegistry& /*assets*/) {
  CheckKeys(data,
            {"direction", "color", "intensity", "sky_color", "ground_color"});
  DirectionalLight light{};
  if (data.contains("direction")) {
    light.direction = ReadVec3(data.at("direction"));
  }
  if (data.contains("color")) {
    light.color = ReadVec3(data.at("color"));
  }
  if (data.contains("intensity")) {
    light.intensity = data.at("intensity").get<float>();
  }
  if (data.contains("sky_color")) {
    light.sky_color = ReadVec3(data.at("sky_color"));
  }
  if (data.contains("ground_color")) {
    light.ground_color = ReadVec3(data.at("ground_color"));
  }
  ecs.AddComponent(entity, light);
}

/** @brief Adds a Collider; it takes no fields, since the mesh sets its size. */
void LoadCollider(const json& data, Entity entity, Ecs& ecs,
                  AssetRegistry& /*assets*/) {
  CheckKeys(data, {});
  ecs.AddComponent(entity, Collider{});
}

}  // namespace

void RegisterBuiltinLoaders(SceneLoader& loader) {
  loader.Register("transform", LoadTransform);
  loader.Register("renderable", LoadRenderable);
  loader.Register("directional_light", LoadDirectionalLight);
  loader.Register("collider", LoadCollider);
}

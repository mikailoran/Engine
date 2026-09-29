#include "builtin_loaders.h"

#include "json_read.h"
#include "scene_loader.h"

#include "../ecs/components/renderable.h"
#include "../ecs/components/transform.h"
#include "../ecs/core/ecs.h"
#include "../resource/asset_registry.h"

#include <bx/math.h>
#include <nlohmann/json.hpp>
#include <string>

namespace {

using nlohmann::json;

/** @brief Adds a Transform; every field is optional. Rotation is in degrees. */
void LoadTransform(const json &data, Entity entity, SceneLoadContext &ctx) {
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
  ctx.ecs.AddComponent(entity, transform);
}

/** @brief Adds a Renderable; "mesh" is required, "color" optional. */
void LoadRenderable(const json &data, Entity entity, SceneLoadContext &ctx) {
  CheckKeys(data, {"mesh", "color"});

  Renderable renderable{};
  renderable.mesh_handle =
      ctx.assets.LoadMesh(data.at("mesh").get<std::string>());
  if (data.contains("color")) {
    renderable.color = ReadFloats<4>(data.at("color"));
  }
  ctx.ecs.AddComponent(entity, renderable);
}

} // namespace

void RegisterBuiltinLoaders(SceneLoader &loader) {
  loader.Register("transform", LoadTransform);
  loader.Register("renderable", LoadRenderable);
}

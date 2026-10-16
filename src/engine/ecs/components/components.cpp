// Compiles every component header on its own, so each stays
// self-contained, and gives clang tooling a file here to take flags from.

#include "engine/ecs/components/animator.h"           // IWYU pragma: keep
#include "engine/ecs/components/attached_meshes.h"    // IWYU pragma: keep
#include "engine/ecs/components/camera.h"             // IWYU pragma: keep
#include "engine/ecs/components/character_body.h"     // IWYU pragma: keep
#include "engine/ecs/components/character_link.h"     // IWYU pragma: keep
#include "engine/ecs/components/collider.h"           // IWYU pragma: keep
#include "engine/ecs/components/directional_light.h"  // IWYU pragma: keep
#include "engine/ecs/components/lifetime.h"           // IWYU pragma: keep
#include "engine/ecs/components/name.h"               // IWYU pragma: keep
#include "engine/ecs/components/physics_link.h"       // IWYU pragma: keep
#include "engine/ecs/components/ragdoll.h"            // IWYU pragma: keep
#include "engine/ecs/components/ragdoll_link.h"       // IWYU pragma: keep
#include "engine/ecs/components/renderable.h"         // IWYU pragma: keep
#include "engine/ecs/components/rigid_body.h"         // IWYU pragma: keep
#include "engine/ecs/components/skin_pose.h"          // IWYU pragma: keep
#include "engine/ecs/components/sky.h"                // IWYU pragma: keep
#include "engine/ecs/components/transform.h"          // IWYU pragma: keep

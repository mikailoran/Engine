cmake_minimum_required(VERSION 3.25)
project(engine LANGUAGES CXX)

# Fail explicitly on unfetched submodules.
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/bgfx.cmake/CMakeLists.txt)
  message(FATAL_ERROR
    "third_party/bgfx.cmake is empty. Run:\n"
    "  git submodule update --init --recursive")
endif()

# Build build/compile_commands.json for clangd and clang-tidy.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# No RTTI anywhere; directory-wide so every dependency below inherits it
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)

# Opt-in bgfx samples; each builds only by name (e.g. example-01-cubes)
option(ENGINE_BGFX_EXAMPLES "Define bgfx's example targets (built only on request)" OFF)
set(BGFX_BUILD_EXAMPLES ${ENGINE_BGFX_EXAMPLES} CACHE BOOL "" FORCE)
# example-common builds only for bgfx's own tools (geometryv, texturev); the
# engine compiles the few helpers it uses itself (bgfx_helpers below)
set(BGFX_BUILD_EXAMPLE_COMMON ON CACHE BOOL "" FORCE)
# Tools stay ON for shaderc, and the wrapper ties example-common to them
set(BGFX_BUILD_TOOLS ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/bgfx.cmake SYSTEM)

# --- ECS core ---------------------------------------------------------------
add_library(ecs_core STATIC
  src/engine/ecs/core/ecs.cpp
  src/engine/ecs/core/entity_manager.cpp
  src/engine/ecs/core/component_array.h
  src/engine/ecs/core/component_manager.h
  src/engine/ecs/core/ecs.h
  src/engine/ecs/core/entity_manager.h
  src/engine/ecs/core/types.h
  src/engine/ecs/core/view.h)

# PUBLIC so both engine and the tests can #include "ecs/core/ecs.h".
target_include_directories(ecs_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_options(ecs_core PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(ecs_core PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

# --- Math -------------------------------------------------------------------
# Rotation conventions shared by rendering, scenes and physics. A library so
# math_tests can link it without the engine.
add_library(engine_math STATIC
  src/engine/math/rotation.cpp
  src/engine/math/rotation.h)

target_include_directories(engine_math PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(engine_math PUBLIC bx)
target_compile_options(engine_math PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(engine_math PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

# --- nlohmann/json ----------------------------------------------------------
include(FetchContent)
FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SYSTEM)
FetchContent_MakeAvailable(nlohmann_json)

# --- Jolt Physics -----------------------------------------------------------
FetchContent_Declare(JoltPhysics
  URL https://github.com/jrouwe/JoltPhysics/archive/refs/tags/v5.6.0.tar.gz
  URL_HASH SHA256=6e069ee0172478cc78182047aac87e5310ba14a67a53348ae14cc37801fd3f8e
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR Build
  SYSTEM)
# LTO on Jolt alone breaks the Release link of a non-LTO engine
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
# GPU/CPU compute only serves Jolt's hair simulation
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(JoltPhysics)

# --- Physics ----------------------------------------------------------------
# PhysicsWorld's headers are Jolt-free; PRIVATE keeps Jolt's include paths and
# JPH_* defines out of every target that links this.
add_library(engine_physics STATIC
  src/engine/physics/body_handle.h
  src/engine/physics/jolt_runtime.h
  src/engine/physics/jolt_runtime.cpp
  src/engine/physics/layers.h
  src/engine/physics/layers.cpp
  src/engine/physics/physics_world.h
  src/engine/physics/shape.h
  src/engine/physics/physics_world.cpp)

target_include_directories(engine_physics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(engine_physics PUBLIC bx PRIVATE Jolt)
target_compile_options(engine_physics PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(engine_physics PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

# --- Physics system ---------------------------------------------------------
# The ECS side of physics. Needs no bgfx, so physics_tests can link it without
# example-common.
add_library(engine_physics_system STATIC
  src/engine/ecs/systems/physics_system.h
  src/engine/ecs/systems/physics_system.cpp)

target_link_libraries(engine_physics_system PUBLIC ecs_core engine_physics)
target_compile_options(engine_physics_system PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(engine_physics_system PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

# --- Runtime layout ---------------------------------------------------------
# Everything loaded at runtime is generated, and it is emitted here, beside the
# executable. The app then derives its asset root from its own location
# (src/engine/platform/asset_root.cpp) instead of a path fixed at build time,
# so the build tree is relocatable and the working directory does not matter.
#
# Pinned explicitly rather than relying on the default being
# CMAKE_CURRENT_BINARY_DIR: the shader and mesh outputs below have to name the
# same directory, and a single-config generator is assumed (a multi-config one
# would put the executable in a per-config subdirectory).
set(ENGINE_RUNTIME_DIR ${CMAKE_BINARY_DIR})

# --- bgfx helpers -----------------------------------------------------------
# The parts of bgfx's example code the engine uses. bgfx_utils.cpp still calls
# entry::getFileReader/getAllocator; src/engine/platform/bgfx_file_access.cpp
# defines them. The source lists bgfx.cmake builds are scoped to its
# directory, so they are globbed again here.
set(BGFX_EXAMPLES_COMMON_DIR ${BGFX_DIR}/examples/common)
file(GLOB MESHOPTIMIZER_SOURCES ${BGFX_DIR}/3rdparty/meshoptimizer/src/*.cpp)
file(GLOB DEAR_IMGUI_SOURCES ${BGFX_DIR}/3rdparty/dear-imgui/*.cpp)

add_library(bgfx_helpers STATIC
  ${BGFX_EXAMPLES_COMMON_DIR}/bgfx_utils.cpp
  ${BGFX_EXAMPLES_COMMON_DIR}/debugdraw/debugdraw.cpp
  ${MESHOPTIMIZER_SOURCES})
target_include_directories(bgfx_helpers SYSTEM PUBLIC
  ${BGFX_EXAMPLES_COMMON_DIR} ${BGFX_DIR}/3rdparty)
target_link_libraries(bgfx_helpers PUBLIC bgfx bx bimg bimg_decode)

# bgfx's ImGui renderer and Dear ImGui, for the game's devtools only
add_library(bgfx_imgui STATIC
  ${BGFX_EXAMPLES_COMMON_DIR}/imgui/imgui.cpp
  ${DEAR_IMGUI_SOURCES})
target_link_libraries(bgfx_imgui PUBLIC bgfx_helpers)

# --- Engine runtime ---------------------------------------------------------
# Everything in src/engine/ that the smaller libraries above don't cover:
# rendering, assets, scenes and the Engine class hosts drive.
add_library(engine_runtime STATIC
  src/engine/engine.h
  src/engine/engine.cpp
  src/engine/bgfx_context.h
  src/engine/bgfx_context.cpp
  src/engine/ecs/components/camera.h
  src/engine/ecs/components/collider.h
  src/engine/ecs/components/physics_link.h
  src/engine/ecs/components/renderable.h
  src/engine/ecs/components/directional_light.h
  src/engine/ecs/components/rigid_body.h
  src/engine/ecs/components/transform.h
  src/engine/ecs/systems/debug_draw_system.h
  src/engine/ecs/systems/debug_draw_system.cpp
  src/engine/ecs/systems/lighting_system.h
  src/engine/ecs/systems/lighting_system.cpp
  src/engine/ecs/systems/render_system.h
  src/engine/ecs/systems/render_system.cpp
  src/engine/resource/mesh_handle.h
  src/engine/resource/texture_handle.h
  src/engine/resource/asset_registry.h
  src/engine/resource/asset_registry.cpp
  src/engine/platform/asset_root.h
  src/engine/platform/asset_root.cpp
  src/engine/platform/bgfx_file_access.h
  src/engine/platform/bgfx_file_access.cpp
  src/engine/platform/frame_context.h
  src/engine/platform/input.h
  src/engine/platform/native_surface.h
  src/engine/platform/screen.h
  src/engine/scene/scene_loader.h
  src/engine/scene/scene_loader.cpp
  src/engine/scene/builtin_loaders.h
  src/engine/scene/builtin_loaders.cpp
  src/engine/scene/json_read.h
  src/engine/scene/picking.h
  src/engine/scene/picking.cpp)

# bgfx_helpers publicly carries bgfx, bx and bimg with their include
# directories, which engine headers need.
target_link_libraries(engine_runtime PUBLIC bgfx_helpers ecs_core engine_math
  engine_physics engine_physics_system nlohmann_json::nlohmann_json)

target_compile_options(engine_runtime PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(engine_runtime PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

# --- Game -------------------------------------------------------------------
# The host: window, input, devtools and the frame loop around Engine.
add_executable(game src/game/main.cpp
  src/game/window.h
  src/game/window.cpp
  src/devtools/selected.h
  src/devtools/fly_camera_system.cpp
  src/devtools/fly_camera_system.h
  src/devtools/selection_system.h
  src/devtools/selection_system.cpp
  src/devtools/ui.h
  src/devtools/ui.cpp)

find_package(SDL3 CONFIG REQUIRED)
target_link_libraries(game PRIVATE engine_runtime bgfx_imgui SDL3::SDL3)

target_compile_options(game PRIVATE -Wall -Wextra -Wpedantic)

# Set as target properties rather than directory-level CMAKE_CXX_* variables, so
# the choice cannot reach anything add_subdirectory() pulls in. bgfx.cmake sets
# its own standard regardless, but this keeps the two independent.
#
# Not target_compile_features(cxx_std_20): that only requires a *minimum*, and
# GCC 16 already defaults to C++20, so CMake emitted no -std flag at all and
# CXX_EXTENSIONS OFF was silently ignored. CXX_STANDARD with STANDARD_REQUIRED
# emits the flag unconditionally, which is what turns extensions off.
set_target_properties(game PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  RUNTIME_OUTPUT_DIRECTORY ${ENGINE_RUNTIME_DIR}
)

# --- Shaders ----------------------------------------------------------------
# bgfxToolUtils.cmake defines bgfx_compile_shaders(). Included explicitly rather
# than relying on the wrapper having pulled it in; the file is guarded against
# double inclusion.
include(${CMAKE_CURRENT_SOURCE_DIR}/third_party/bgfx.cmake/cmake/bgfxToolUtils.cmake)

# INCLUDE_DIRS is mandatory here: bgfx_compile_shaders passes an undefined
# BGFX_SHADER_INCLUDE_PATH, so bgfx/src -- which holds bgfx_shader.sh -- has to
# be named or #include <bgfx_shader.sh> cannot resolve.
set(BGFX_SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/bgfx.cmake/bgfx/src)

# Output lands in <OUTPUT_DIR>/<profile>/<name>.sc.bin, matching the layout
# loadShader expects: shaders/spirv/, shaders/glsl/, shaders/essl/. Emitted under
# ENGINE_RUNTIME_DIR so it sits beside the executable, which is where the app's
# asset root points. bgfx_compile_shaders mkdirs each profile directory itself.
bgfx_compile_shaders(
  TYPE VERTEX
  SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/shaders/vs_mesh.sc
  VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/shaders/varying.def.sc
  OUTPUT_DIR ${ENGINE_RUNTIME_DIR}/shaders
  INCLUDE_DIRS ${BGFX_SHADER_DIR}
  OUT_FILES_VAR VS_OUTPUTS
)

bgfx_compile_shaders(
  TYPE FRAGMENT
  SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/shaders/fs_mesh.sc
  VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/shaders/varying.def.sc
  OUTPUT_DIR ${ENGINE_RUNTIME_DIR}/shaders
  INCLUDE_DIRS ${BGFX_SHADER_DIR}
  OUT_FILES_VAR FS_OUTPUTS
)

# add_custom_command outputs are only built when a target depends on them.
add_custom_target(shaders DEPENDS ${VS_OUTPUTS} ${FS_OUTPUTS})
add_dependencies(game shaders)

# --- Meshes -----------------------------------------------------------------
# Unlike bgfx_compile_shaders, bgfx_compile_geometry emits a bare custom command
# with no mkdir, and geometryc will not create the directory either -- so create
# it at configure time. The build tree persists from configure to build, so once
# is enough.
set(ENGINE_MESH_DIR ${ENGINE_RUNTIME_DIR}/assets/meshes)
file(MAKE_DIRECTORY ${ENGINE_MESH_DIR})

# Base names of the meshes to compile
set(ENGINE_MESHES
  plane
  cube
  sphere
  cylinder
  cone
  torus
  bunny
)

# One geometryc command per mesh
set(ENGINE_MESH_OUTPUTS "")
foreach(mesh IN LISTS ENGINE_MESHES)
  bgfx_compile_geometry(
    FILE ${CMAKE_CURRENT_SOURCE_DIR}/assets/meshes/${mesh}.obj
    OUTPUT ${ENGINE_MESH_DIR}/${mesh}.bin
    PACKNORMAL 1
  )
  list(APPEND ENGINE_MESH_OUTPUTS ${ENGINE_MESH_DIR}/${mesh}.bin)
endforeach()
add_custom_target(meshes DEPENDS ${ENGINE_MESH_OUTPUTS})
add_dependencies(game meshes)

# --- Textures ---------------------------------------------------------------
# bgfx_compile_texture is a bare custom command like geometry: create the
# output directory at configure time.
set(ENGINE_TEXTURE_DIR ${ENGINE_RUNTIME_DIR}/assets/textures)
file(MAKE_DIRECTORY ${ENGINE_TEXTURE_DIR})

# Base names of the textures to compile
set(ENGINE_TEXTURES
  debug_grid
)

# One texturec command per texture, with a full mip chain. RGBA8 because
# Vulkan cannot sample RGB8 as sRGB, which bgfx treats as fatal.
set(ENGINE_TEXTURE_OUTPUTS "")
foreach(texture IN LISTS ENGINE_TEXTURES)
  bgfx_compile_texture(
    FILE ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures/${texture}.png
    OUTPUT ${ENGINE_TEXTURE_DIR}/${texture}.dds
    FORMAT RGBA8
    MIPS
  )
  list(APPEND ENGINE_TEXTURE_OUTPUTS ${ENGINE_TEXTURE_DIR}/${texture}.dds)
endforeach()
add_custom_target(textures DEPENDS ${ENGINE_TEXTURE_OUTPUTS})
add_dependencies(game textures)

# --- Scenes -----------------------------------------------------------------
# Scene files are copied beside the executable; edits recopy on build.
set(ENGINE_SCENE_DIR ${ENGINE_RUNTIME_DIR}/assets/scenes)

# Base names of the scene files to copy
set(ENGINE_SCENES
  debug
)

set(ENGINE_SCENE_OUTPUTS "")
foreach(scene IN LISTS ENGINE_SCENES)
  set(scene_src ${CMAKE_CURRENT_SOURCE_DIR}/assets/scenes/${scene}.json)
  set(scene_out ${ENGINE_SCENE_DIR}/${scene}.json)
  add_custom_command(
    OUTPUT ${scene_out}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ENGINE_SCENE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${scene_src} ${scene_out}
    MAIN_DEPENDENCY ${scene_src}
  )
  list(APPEND ENGINE_SCENE_OUTPUTS ${scene_out})
endforeach()
add_custom_target(scenes DEPENDS ${ENGINE_SCENE_OUTPUTS})
add_dependencies(game scenes)

# --- Tests ------------------------------------------------------------------
# Off-switch so a plain game build never triggers googletest's network fetch.
option(ENGINE_BUILD_TESTS "Build the ECS unit tests" ON)
if(ENGINE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# --- Git hooks --------------------------------------------------------------
option(ENGINE_INSTALL_GIT_HOOKS "Point git's core.hooksPath at .githooks" ON)

# Skipped outside a git checkout (a tarball export, say) and when this project
# is consumed as a subproject -- neither has hooks to install. The top-level
# test is spelled out rather than using PROJECT_IS_TOP_LEVEL, which needs 3.21.
if(ENGINE_INSTALL_GIT_HOOKS
  AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git
  AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  find_package(Git QUIET)
  if(NOT GIT_FOUND)
    message(STATUS "git not found -- leaving core.hooksPath unset")
  else()
    # Read the current hooksPath value first so an explicit local override is not 
    # rewritten on every reconfigure.
    execute_process(
      COMMAND ${GIT_EXECUTABLE} config --get core.hooksPath
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      OUTPUT_VARIABLE ENGINE_CURRENT_HOOKS_PATH
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)

    if(ENGINE_CURRENT_HOOKS_PATH STREQUAL ".githooks")
      message(STATUS "git hooks already active (core.hooksPath=.githooks)")
    elseif(NOT ENGINE_CURRENT_HOOKS_PATH STREQUAL "")
      message(WARNING
        "core.hooksPath is set to '${ENGINE_CURRENT_HOOKS_PATH}', not "
        ".githooks -- leaving it alone. The unit-test hooks will not run. "
        "Set it manually or configure with -DENGINE_INSTALL_GIT_HOOKS=OFF to "
        "silence this.")
    else()
      execute_process(
        COMMAND ${GIT_EXECUTABLE} config core.hooksPath .githooks
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE ENGINE_HOOKS_RESULT)
      if(ENGINE_HOOKS_RESULT EQUAL 0)
        message(STATUS "git hooks installed: core.hooksPath=.githooks")
      else()
        message(WARNING "failed to set core.hooksPath -- hooks will not run")
      endif()
    endif()
  endif()
endif()

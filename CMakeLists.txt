cmake_minimum_required(VERSION 3.20)
project(engine LANGUAGES CXX)

# Fail explicitly on unfetched submodules.
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/bgfx.cmake/CMakeLists.txt)
  message(FATAL_ERROR
    "third_party/bgfx.cmake is empty. Run:\n"
    "  git submodule update --init --recursive")
endif()

# Build build/compile_commands.json for clangd and clang-tidy.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Configure the bgfx.cmake wrapper before pulling it in. The ~50 sample
# executables are skipped, but example-common is still requested: it carries the
# entry platform layer that main.cpp uses for windowing and input.
# BGFX_BUILD_TOOLS stays ON both for shaderc and because the wrapper forces
# example-common ON alongside it.
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_EXAMPLE_COMMON ON CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/bgfx.cmake)

# --- ECS core ---------------------------------------------------------------
add_library(ecs_core STATIC
  src/ecs/core/ecs.cpp
  src/ecs/core/entity_manager.cpp
  src/ecs/core/system_manager.cpp
  src/ecs/core/component_array.h
  src/ecs/core/component_manager.h
  src/ecs/core/ecs.h
  src/ecs/core/entity_manager.h
  src/ecs/core/frame_context.h
  src/ecs/core/system_manager.h
  src/ecs/core/system.h
  src/ecs/core/types.h)

# PUBLIC so both engine and the tests can #include "ecs/core/ecs.h".
target_include_directories(ecs_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_options(ecs_core PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(ecs_core PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

# --- Runtime layout ---------------------------------------------------------
# Everything loaded at runtime is generated, and it is emitted here, beside the
# executable. The app then derives its asset root from its own location
# (src/platform/asset_root.cpp) instead of a path fixed at build time, so the
# build tree is relocatable and the working directory does not matter.
#
# Pinned explicitly rather than relying on the default being
# CMAKE_CURRENT_BINARY_DIR: the shader and mesh outputs below have to name the
# same directory, and a single-config generator is assumed (a multi-config one
# would put the executable in a per-config subdirectory).
set(ENGINE_RUNTIME_DIR ${CMAKE_BINARY_DIR})

add_executable(engine src/main.cpp
  src/ecs/components/camera.h
  src/ecs/components/gravity.h
  src/ecs/components/renderable.h
  src/ecs/components/rigid_body.h
  src/ecs/components/spin.h
  src/ecs/components/transform.h
  src/ecs/systems/camera_control.cpp
  src/ecs/systems/camera_control.h
  src/ecs/systems/physics_system.h
  src/ecs/systems/physics_system.cpp
  src/ecs/systems/render_system.h
  src/ecs/systems/render_system.cpp
  src/assets/mesh_handle.h
  src/assets/asset_registry.h
  src/assets/asset_registry.cpp
  src/platform/asset_root.h
  src/platform/asset_root.cpp)

# example-common publicly exposes examples/common plus bgfx, bx and bimg with
# their include directories, which is what makes "entry/entry.h" resolve.
target_link_libraries(engine PRIVATE example-common ecs_core)

target_compile_options(engine PRIVATE -Wall -Wextra -Wpedantic)

# Set as target properties rather than directory-level CMAKE_CXX_* variables, so
# the choice cannot reach anything add_subdirectory() pulls in. bgfx.cmake sets
# its own standard regardless, but this keeps the two independent.
#
# Not target_compile_features(cxx_std_20): that only requires a *minimum*, and
# GCC 16 already defaults to C++20, so CMake emitted no -std flag at all and
# CXX_EXTENSIONS OFF was silently ignored. CXX_STANDARD with STANDARD_REQUIRED
# emits the flag unconditionally, which is what turns extensions off.
set_target_properties(engine PROPERTIES
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
  SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/vs.sc
  VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/src/varying.def.sc
  OUTPUT_DIR ${ENGINE_RUNTIME_DIR}/shaders
  INCLUDE_DIRS ${BGFX_SHADER_DIR}
  OUT_FILES_VAR VS_OUTPUTS
)

bgfx_compile_shaders(
  TYPE FRAGMENT
  SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/fs.sc
  VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/src/varying.def.sc
  OUTPUT_DIR ${ENGINE_RUNTIME_DIR}/shaders
  INCLUDE_DIRS ${BGFX_SHADER_DIR}
  OUT_FILES_VAR FS_OUTPUTS
)

# add_custom_command outputs are only built when a target depends on them.
add_custom_target(shaders DEPENDS ${VS_OUTPUTS} ${FS_OUTPUTS})
add_dependencies(engine shaders)

# --- Meshes -----------------------------------------------------------------
# Unlike bgfx_compile_shaders, bgfx_compile_geometry emits a bare custom command
# with no mkdir, and geometryc will not create the directory either -- so create
# it at configure time. The build tree persists from configure to build, so once
# is enough.
set(ENGINE_MESH_DIR ${ENGINE_RUNTIME_DIR}/assets/meshes/compiled)
file(MAKE_DIRECTORY ${ENGINE_MESH_DIR})

bgfx_compile_geometry(
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/assets/meshes/bunny.obj
  OUTPUT ${ENGINE_MESH_DIR}/bunny.bin
)
add_custom_target(meshes DEPENDS ${ENGINE_MESH_DIR}/bunny.bin)
add_dependencies(engine meshes)

# --- Tests ------------------------------------------------------------------
# Off-switch so a plain game build never triggers googletest's network fetch.
option(ENGINE_BUILD_TESTS "Build the ECS unit tests" ON)
if(ENGINE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

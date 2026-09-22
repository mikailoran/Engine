cmake_minimum_required(VERSION 3.20)
project(mygame LANGUAGES CXX)

# Emit build/compile_commands.json. clangd and clang-tidy both read it to learn
# each translation unit's real flags; without it they fall back to guesses.
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

add_executable(mygame src/main.cpp
  src/ecs/core/component_array.h
  src/ecs/core/component_manager.h
  src/ecs/core/ecs.cpp
  src/ecs/core/ecs.h
  src/ecs/core/entity_manager.cpp
  src/ecs/core/entity_manager.h
  src/ecs/core/system_manager.h
  src/ecs/core/system.h
  src/ecs/core/types.h
  src/ecs/components/camera.h
  src/ecs/components/gravity.h
  src/ecs/components/rigid_body.h
  src/ecs/components/spin.h
  src/ecs/components/transform.h
  src/ecs/systems/camera_control.cpp
  src/ecs/systems/camera_control.h
  src/ecs/core/system_manager.cpp
  src/ecs/systems/physics_system.h
  src/ecs/systems/physics_system.cpp
  src/ecs/systems/render_system.h
  src/ecs/systems/render_system.cpp)

# example-common publicly exposes examples/common plus bgfx, bx and bimg with
# their include directories, which is what makes "entry/entry.h" resolve.
target_link_libraries(mygame PRIVATE example-common)

target_compile_options(mygame PRIVATE -Wall -Wextra -Wpedantic)

# Set as target properties rather than directory-level CMAKE_CXX_* variables, so
# the choice cannot reach anything add_subdirectory() pulls in. bgfx.cmake sets
# its own standard regardless, but this keeps the two independent.
#
# Not target_compile_features(cxx_std_20): that only requires a *minimum*, and
# GCC 16 already defaults to C++20, so CMake emitted no -std flag at all and
# CXX_EXTENSIONS OFF was silently ignored. CXX_STANDARD with STANDARD_REQUIRED
# emits the flag unconditionally, which is what turns extensions off.
set_target_properties(mygame PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
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

# Output lands in <OUTPUT_DIR>/<profile>/<name>.sc.bin, matching the layout the
# example loader expects: shaders/spirv/, shaders/glsl/, shaders/essl/.
# Kept in the source tree so it resolves from the working directory the app runs
# with, which is the repository root.
bgfx_compile_shaders(
  TYPE VERTEX
  SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/vs.sc
  VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/src/varying.def.sc
  OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders # TODO: move to build tree?
  INCLUDE_DIRS ${BGFX_SHADER_DIR}
  OUT_FILES_VAR VS_OUTPUTS
)

bgfx_compile_shaders(
  TYPE FRAGMENT
  SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/fs.sc
  VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/src/varying.def.sc
  OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders
  INCLUDE_DIRS ${BGFX_SHADER_DIR}
  OUT_FILES_VAR FS_OUTPUTS
)

# add_custom_command outputs are only built when a target depends on them.
add_custom_target(shaders DEPENDS ${VS_OUTPUTS} ${FS_OUTPUTS})
add_dependencies(mygame shaders)

# Compile meshes
bgfx_compile_geometry(
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/assets/meshes/bunny.obj
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/assets/meshes/compiled/bunny.bin
)
add_custom_target(meshes DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/meshes/compiled/bunny.bin)
add_dependencies(mygame meshes)

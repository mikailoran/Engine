# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
  SYSTEM)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(ecs_tests
  ecs/entity_test.cpp
  ecs/component_test.cpp
  ecs/view_test.cpp
  ecs/destruction_test.cpp
  ecs/deferred_destruction_test.cpp)

target_link_libraries(ecs_tests PRIVATE ecs_core GTest::gtest_main)

target_compile_options(ecs_tests PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(ecs_tests PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

add_executable(math_tests
  math/rotation_test.cpp
  math/camera_test.cpp
  math/shadow_fit_test.cpp)

target_link_libraries(math_tests PRIVATE engine_math GTest::gtest_main)

target_compile_options(math_tests PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(math_tests PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

add_executable(physics_tests
  physics/character_test.cpp
  physics/ray_cast_test.cpp
  physics/physics_system_test.cpp)

target_link_libraries(physics_tests PRIVATE engine_physics_system engine_math
  GTest::gtest_main)

target_compile_options(physics_tests PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(physics_tests PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

add_executable(asset_tests
  assets/gltf_reader_test.cpp
  assets/bone_parts_test.cpp
  assets/skeleton_test.cpp
  assets/animation_clip_test.cpp
  assets/retarget_test.cpp
  assets/prefab_clips_test.cpp)

target_link_libraries(asset_tests PRIVATE engine_assets nlohmann_json::nlohmann_json
  GTest::gtest_main)
# Reads the source tree's assets, not the copies beside the executable
target_compile_definitions(asset_tests PRIVATE
  ENGINE_ASSET_DIR="${PROJECT_SOURCE_DIR}/assets")

target_compile_options(asset_tests PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(asset_tests PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

# The animation runtime on the real rigs and clips; no bgfx or ECS
add_executable(animation_tests
  animation/rig_test.cpp
  animation/evaluate_test.cpp
  animation/playback_test.cpp
  animation/mask_test.cpp)

target_link_libraries(animation_tests PRIVATE engine_animation GTest::gtest_main)
target_compile_definitions(animation_tests PRIVATE
  ENGINE_ASSET_DIR="${PROJECT_SOURCE_DIR}/assets")

target_compile_options(animation_tests PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(animation_tests PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

# Gameplay rules that need no engine: header-only, so only src/ is needed
add_executable(game_tests
  game/health_test.cpp
  game/enemy_rules_test.cpp)

target_include_directories(game_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(game_tests PRIVATE GTest::gtest_main)

target_compile_options(game_tests PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(game_tests PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

include(GoogleTest)
gtest_discover_tests(ecs_tests)
gtest_discover_tests(math_tests)
gtest_discover_tests(physics_tests)
gtest_discover_tests(asset_tests)
gtest_discover_tests(animation_tests)
gtest_discover_tests(game_tests)

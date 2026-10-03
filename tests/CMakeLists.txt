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

include(GoogleTest)
gtest_discover_tests(ecs_tests)

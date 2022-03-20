cmake_minimum_required(VERSION 3.18)

project(
  net
  VERSION 1.0
  LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -pedantic -Werror)

add_subdirectory(externals/spdlog)

add_library(
  core
  src/http.cpp
  src/http.hpp
  src/network.cpp
  src/network.hpp
)

target_link_libraries(
  core
  PRIVATE
  spdlog
)

add_executable(
  net
  src/main.cpp
)

target_link_libraries(
  net
  PRIVATE
  core
  spdlog
)

add_subdirectory(externals/googletest)
enable_testing()

add_executable(
  all_tests
  tests/http_tests.cpp
)

target_include_directories(
  all_tests
  PUBLIC
  src
)

target_link_libraries(
  all_tests
  gtest_main
  gmock
  core
)

include(GoogleTest)
gtest_discover_tests(all_tests)

add_custom_target(
  format
  COMMAND git ls-files '*.cpp' '*.hpp' | xargs clang-format -style=file -i
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

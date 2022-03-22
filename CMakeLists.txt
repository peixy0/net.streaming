cmake_minimum_required(VERSION 3.18)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -pedantic -Werror)

option(BUILD_WITH_ADDRESS_SANITIZER "Build with address sanitize flags" OFF)
option(BUILD_WITH_MEMORY_SANITIZER "Build with memory sanitize flags" OFF)

if (BUILD_WITH_ADDRESS_SANITIZER)
  set(CMAKE_C_COMPILER "/usr/bin/clang")
  set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
  add_compile_options(-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address -fsanitize=undefined)
endif()

if (BUILD_WITH_MEMORY_SANITIZER)
  set(CMAKE_C_COMPILER "/usr/bin/clang")
  set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
  add_compile_options(-fsanitize=memory -fno-omit-frame-pointer)
  add_link_options(-fsanitize=memory)
endif()

add_subdirectory(externals/spdlog)

project(
  net
  VERSION 1.0
)

add_library(
  core
  src/app.cpp
  src/app.hpp
  src/http.cpp
  src/http.hpp
  src/network.hpp
  src/tcp.cpp
  src/tcp.hpp
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
  PRIVATE
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

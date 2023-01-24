cmake_minimum_required(VERSION 3.18)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -pedantic)

option(BUILD_STATIC "Build static flag" OFF)
option(BUILD_WITH_ADDRESS_SANITIZER "Build with address sanitize flags" OFF)
option(BUILD_WITH_MEMORY_SANITIZER "Build with memory sanitize flags" OFF)

if (BUILD_STATIC)
  add_compile_options(-static)
  add_link_options(-static -Wl,--whole-archive -lpthread -Wl,--no-whole-archive)
endif()

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
set(YAML_CPP_BUILD_TOOLS OFF CACHE INTERNAL "No build tools")
set(YAML_CPP_FORMAT_SOURCE OFF CACHE INTERNAL "No format source")
set(YAML_CPP_BUILD_TESTS OFF CACHE INTERNAL "No build tests")
set(YAML_CPP_BUILD_TESTS OFF CACHE INTERNAL "No build tests")
add_subdirectory(externals/yaml-cpp)

project(
  net.streaming
  VERSION 1.0
)

find_library(AVCODEC_LIBRARY avcodec)
find_library(AVUTIL_LIBRARY avutil)
find_library(AVFILTER_LIBRARY avfilter)
find_library(AVFORMAT_LIBRARY avformat)

add_subdirectory(src)

add_custom_target(
  format
  COMMAND git ls-files '*.cpp' '*.hpp' | xargs clang-format -style=file -i
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

if (BUILD_TESTING)
  add_subdirectory(externals/googletest)
  include(GoogleTest)
  enable_testing()
  add_subdirectory(tests)
endif()

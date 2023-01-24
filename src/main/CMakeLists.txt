add_executable(
  app
  app.cpp
  app.hpp
  main.cpp
  stream.cpp
  stream.hpp
)

target_link_libraries(
  app
  PRIVATE
  core
  spdlog
  yaml-cpp
)

set_target_properties(
  app
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY
  "${CMAKE_BINARY_DIR}"
)

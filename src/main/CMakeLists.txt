add_executable(
  net
  main.cpp
  app.cpp
  app.hpp
)

target_link_libraries(
  net
  PRIVATE
  net.core
  spdlog
  nlohmann_json
)

set_target_properties(
  net
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY
  "${CMAKE_BINARY_DIR}"
)

add_executable(
  net
  app.cpp
  app.hpp
  main.cpp
  video.cpp
  video.hpp
)

target_link_libraries(
  net
  PRIVATE
  net.core
  spdlog
)

set_target_properties(
  net
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY
  "${CMAKE_BINARY_DIR}"
)

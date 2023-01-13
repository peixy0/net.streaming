add_library(
  core
  file.cpp
  file.hpp
  http.cpp
  http.hpp
  network.hpp
  parser.cpp
  parser.hpp
  tcp.cpp
  tcp.hpp

  codec.cpp
  codec.hpp
  event_queue.hpp
  video.cpp
  video.hpp
)

target_link_libraries(
  core
  PRIVATE
  spdlog
)

target_include_directories(
  core
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

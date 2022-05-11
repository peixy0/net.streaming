add_library(
  net.core
  file.cpp
  file.hpp
  http.cpp
  http.hpp
  network.hpp
  parser.cpp
  parser.hpp
  tcp.cpp
  tcp.hpp
)

target_link_libraries(
  net.core
  PRIVATE
  spdlog
)

target_include_directories(
  net.core
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

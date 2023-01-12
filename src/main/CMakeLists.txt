add_executable(
  net
  app.cpp
  app.hpp
  codec.cpp
  codec.hpp
  main.cpp
  video.cpp
  video.hpp
)

target_link_libraries(
  net
  PRIVATE
  net.core
  spdlog
  ${AVCODEC_LIBRARY}
  ${AVUTIL_LIBRARY}
  ${AVFILTER_LIBRARY}
  ${AVFORMAT_LIBRARY}
)

set_target_properties(
  net
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY
  "${CMAKE_BINARY_DIR}"
)

add_executable(
  all_tests
  network_tests.cpp
)

target_include_directories(
  all_tests
  PUBLIC
  ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(
  all_tests
  PRIVATE
  gtest_main
  gmock
  spdlog
  core
)

set_target_properties(
  all_tests
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY
  "${CMAKE_BINARY_DIR}"
)

gtest_discover_tests(all_tests)

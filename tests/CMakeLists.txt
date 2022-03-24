add_executable(
  all_tests
  network_mocks.hpp
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

gtest_discover_tests(all_tests)

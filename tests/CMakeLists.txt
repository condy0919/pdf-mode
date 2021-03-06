cmake_minimum_required(VERSION 3.11)

project(yapdf-tests)

add_library(bridge_tests SHARED
  bridge_tests.cpp
)
target_link_libraries(bridge_tests LINK_PRIVATE
  doctest::doctest
  yapdf::yapdf
)
add_test(NAME BridgeTests
  COMMAND emacs -Q --batch -l $<TARGET_FILE:bridge_tests> --module-assertions
)

add_executable(expected_tests
  expected_tests.cpp
)
target_link_libraries(expected_tests LINK_PRIVATE
  doctest_with_main
  yapdf::yapdf
)
add_test(NAME ExpectedTests
  COMMAND $<TARGET_FILE:expected_tests>
)

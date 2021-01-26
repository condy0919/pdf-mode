cmake_minimum_required(VERSION 3.11)

project(yapdf-tests)

add_library(bridge_tests SHARED
  "bridge_test.cpp"
)
target_link_libraries(bridge_tests LINK_PRIVATE
  doctest::doctest
  yapdf::yapdf
)
add_test(NAME BridgeTests
  COMMAND emacs -Q -l libbridge_tests.so --module-assertions -f "kill-emacs"
)

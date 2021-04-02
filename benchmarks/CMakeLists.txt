cmake_minimum_required(VERSION 3.11)

project(yapdf-benchmarks)

add_library(bridge_benchmarks SHARED
  bridge_benchmarks.cpp
)
target_link_libraries(bridge_benchmarks PRIVATE
  benchmark::benchmark
  yapdf::yapdf
)
add_test(NAME BridgeBenchmarks
  COMMAND emacs -Q --batch -l $<TARGET_FILE:bridge_benchmarks> --module-assertions
)

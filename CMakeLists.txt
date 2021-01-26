cmake_minimum_required(VERSION 3.11)

include(FetchContent)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

project(yapdf
  VERSION 0.1.0
  LANGUAGES C CXX
  DESCRIPTION ""
  HOMEPAGE_URL ""
)

OPTION(YAPDF_ENABLE_TESTS "Enable tests" ON)
OPTION(YAPDF_ENABLE_BENCHMARKS "Enable benchmarks" OFF)

# Sources for the library are specified at the end
add_library(yapdf SHARED "")

# Alias to avoid name conflicts
add_library(yapdf::yapdf ALIAS yapdf)

### Commpile options
# Enable C++17 (Required)
target_compile_features(yapdf
  PUBLIC
    cxx_std_17
)

# Common GCC/Clang options
target_compile_options(yapdf
  PRIVATE
    -Wall
    -Wextra
)

### Libraries
# Enable threading support
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(yapdf PRIVATE Threads::Threads)

# Benchmark
if(YAPDF_ENABLE_BENCHMARKS)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark testing" FORCE)
  FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.5.2
    GIT_SHALLOW    true
    GIT_PROGRESS   true
  )
  FetchContent_MakeAvailable(benchmark)

  add_subdirectory(benchmarks)
endif()

# Test
if(YAPDF_ENABLE_TESTS)
  enable_testing()

  # libdoctest.a and libdoctest_with_main.a will be built
  FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/onqtam/doctest
    GIT_TAG        2.4.4
    GIT_SHALLOW    true
    GIT_PROGRESS   true
  )
  FetchContent_MakeAvailable(doctest)

  add_subdirectory(tests)
endif()

### Definitions

### Includes

target_include_directories(yapdf PUBLIC include)

### Install
install(TARGETS yapdf LIBRARY DESTINATION lib RUNTIME DESTINATION bin)

### Sources
target_sources(yapdf
  PRIVATE
    src/lib.cpp
)

### Obtain version information from Git
if(NOT YAPDF_VERSION)
  execute_process(COMMAND git describe --tag --long HEAD
    OUTPUT_VARIABLE YAPDF_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

  if(NOT YAPDF_VERSION)
    set(YAPDF_VERSION "<unknown>")
  endif()
endif()

target_compile_definitions(yapdf PRIVATE YAPDF_VERSION=${YAPDF_VERSION})

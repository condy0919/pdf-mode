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

option(YAPDF_ENABLE_TESTS "Enable tests" ON)
option(YAPDF_ENABLE_BENCHMARKS "Enable benchmarks" OFF)

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

# pkg-config integration
find_package(PkgConfig REQUIRED)

# Gtk3
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)
target_include_directories(yapdf PUBLIC ${GTKMM_INCLUDE_DIRS})
target_link_directories(yapdf PUBLIC ${GTKMM_LIBRARY_DIRS})
target_link_libraries(yapdf PUBLIC ${GTKMM_LIBRARIES})

# poppler
pkg_check_modules(POPPLER REQUIRED poppler-cpp)
target_include_directories(yapdf PUBLIC ${POPPLER_INCLUDE_DIRS})
target_link_directories(yapdf PUBLIC ${POPPLER_LIBRARY_DIRS})
target_link_libraries(yapdf PUBLIC ${POPPLER_LIBRARIES})

# Benchmark
if(YAPDF_ENABLE_BENCHMARKS)
  enable_testing()

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
    GIT_TAG        2.4.5
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
    src/bridge.cpp
    src/unreachable.cpp
)

### Obtain version information from Git
if(NOT YAPDF_VERSION)
  execute_process(COMMAND git describe --always --long HEAD
    OUTPUT_VARIABLE YAPDF_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

  if(NOT YAPDF_VERSION)
    set(YAPDF_VERSION "<unknown>")
  endif()
endif()

target_compile_definitions(yapdf PRIVATE YAPDF_VERSION="${YAPDF_VERSION}")

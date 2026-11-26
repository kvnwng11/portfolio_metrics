# Set the minimum required CMake version
cmake_minimum_required(VERSION 3.10)

# Define the project name and programming language
project(PortflioMetrics LANGUAGES CXX)
enable_testing()

# Direct all built executables to build/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

# Enforce a modern C++ standard (e.g., C++23)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(portfolio_core STATIC datalayer.cpp portfolio.cpp)
target_include_directories(portfolio_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(portfolio_metrics main.cpp)
target_link_libraries(portfolio_metrics PRIVATE portfolio_core)

# Create the executable target: target_name source_files
# add_executable(portfolio_metrics main.cpp)
# target_include_directories(portfolio_metrics PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})


include(FetchContent)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)  # provides gtest_discover_tests()

# List of tests to run
add_executable(tests 
  tests/test_current_value.cpp
  tests/test_volatility.cpp
  tests/test_sharpe_ratio.cpp
  tests/test_sortino_ratio.cpp
  tests/test_max_drawdown.cpp
  tests/test_beta.cpp
  tests/test_value_at_risk.cpp
  tests/test_portfolio.cpp
  tests/test_datalayer.cpp
)

target_link_libraries(tests PRIVATE portfolio_core gtest)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

gtest_discover_tests(tests)

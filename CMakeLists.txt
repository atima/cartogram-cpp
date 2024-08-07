cmake_minimum_required(VERSION 3.1)
project(cartogram LANGUAGES CXX)

# ========== Project Setup ==========
set(CMAKE_CXX_STANDARD 20)
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

# ========== Dependencies Setup ==========

# Boost
find_package(Boost REQUIRED COMPONENTS unit_test_framework)

# PkgConfig, fftw, and cairo
find_package(PkgConfig REQUIRED)
pkg_search_module(fftw REQUIRED fftw3 IMPORTED_TARGET)
pkg_search_module(cairo REQUIRED cairo IMPORTED_TARGET)

# ========== Compiler Setup ==========
if(APPLE)
  set(LLVM_BASE_PATH "/usr/local/opt/llvm@17/bin/")

  if(${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "arm64")
    set(LLVM_BASE_PATH "/opt/homebrew/opt/llvm@17/bin/")
  endif()

  set(CMAKE_CXX_COMPILER "${LLVM_BASE_PATH}clang++")
  set(CMAKE_C_COMPILER "${LLVM_BASE_PATH}clang")
elseif(UNIX)
  set(CMAKE_CXX_COMPILER "g++-11")
  set(CMAKE_C_COMPILER "gcc-11")
endif()

# ========== Source Files ==========
file(GLOB_RECURSE CARTOGRAM_SOURCES "src/*.cpp")
add_executable(cartogram ${CARTOGRAM_SOURCES})

# ========== Include Directories ==========
target_include_directories(cartogram PUBLIC
  ${PROJECT_SOURCE_DIR}/include
  ${Boost_INCLUDE_DIRS}
  PkgConfig::fftw
  PkgConfig::cairo
)

# ========== Compile Options ==========
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(cartogram PRIVATE -I ${Boost_INCLUDE_DIRS})
  target_compile_options(cartogram PRIVATE -ffp-contract=off)
elseif(MSVC)
  target_compile_options(cartogram PRIVATE /external:I ${Boost_INCLUDE_DIRS})
endif()

# Compiler warnings
target_compile_options(cartogram PRIVATE -Wall -Wextra -pedantic -Wno-deprecated-declarations)

# ========== Linking Libraries ==========
target_link_libraries(cartogram
  PkgConfig::fftw
  PkgConfig::cairo
)

# ========== Installation ==========
install(TARGETS cartogram DESTINATION bin)

# Enable CTest testing
enable_testing()

# Add test executable
file(GLOB_RECURSE TEST_FILES "tests/*.cpp")

# Include the source files from the src directory that are needed for testing
set(CARTOGRAM_TEST_SOURCES_FROM_SRC
  "src/misc/string_to_decimal_converter.cpp"

  # Add additional test sources from src here if necessary
)

# For each test file, create an executable and a test
foreach(TEST_FILE ${TEST_FILES})
  # Extract the filename without an extension to use as a test name
  get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)

  # Create an executable for each test file
  add_executable(${TEST_NAME} ${TEST_FILE} ${CARTOGRAM_TEST_SOURCES_FROM_SRC})

  # Include directories for the test executable
  target_include_directories(${TEST_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
    PkgConfig::fftw
  )

  if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${TEST_NAME} PRIVATE -I ${Boost_INCLUDE_DIRS})
    target_compile_options(${TEST_NAME} PRIVATE -ffp-contract=off)
  elseif(MSVC)
    target_compile_options(${TEST_NAME} PRIVATE /external:I ${Boost_INCLUDE_DIRS})
  endif()

  # Compiler warnings for the test executable
  target_compile_options(${TEST_NAME} PRIVATE -Wall -Wextra -pedantic -Wno-deprecated-declarations)

  # Linking Libraries for Test Executable
  target_link_libraries(${TEST_NAME}
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
    PkgConfig::fftw
  )

  # Register the executable as a test
  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

# Uninstall target
add_custom_target("uninstall")
add_custom_command(
  TARGET "uninstall"
  POST_BUILD
  COMMENT "Uninstalling cartogram..."
  COMMAND xargs rm -vf < install_manifest.txt || echo "Nothing in install_manifest.txt to be uninstalled!"
)
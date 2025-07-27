cmake_minimum_required (VERSION 3.19)
set(VCPKG_LIBRARY_LINKAGE static)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set (CMAKE_CXX_STANDARD 20)
project (CompressedImageBuild)

# If we are compiling as the main project we automatically turn on all the build options.
# This can be circumvented by passing "-DCOMPRESSED_DETERMINE_MAIN_PROJECT=OFF"
set(MAIN_PROJECT OFF)
option (
    COMPRESSED_DETERMINE_MAIN_PROJECT 
    "Whether to automatically determine if we are building this module as main project" 
    ON
)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR AND COMPRESSED_DETERMINE_MAIN_PROJECT)
    message("Compiling compressed-image as main project")
    set(MAIN_PROJECT ON)
else()
    set(MAIN_PROJECT OFF)
endif()

if (MAIN_PROJECT)
    set(COMPRESSED_IMAGE_USE_VCPKG ON)
    set(COMPRESSED_IMAGE_BUILD_TESTS ON)
    set(COMPRESSED_IMAGE_BUILD_EXAMPLES ON)
    set(COMPRESSED_IMAGE_BUILD_PYTHON ON)
    set(COMPRESSED_IMAGE_BUILD_DOCS ON)
    set(COMPRESSED_IMAGE_BUILD_BENCHMARKS ON)
    set(COMPRESSED_IMAGE_EXTENDED_WARNINGS ON)
endif()

option(COMPRESSED_IMAGE_USE_VCPKG "Whether to use the submodule version of vcpkg to resolve the dependencies instead of system libraries." OFF)
option(COMPRESSED_IMAGE_EXTENDED_WARNINGS "Whether to compile with extended warnings (-Wextra, -Werror etc.)" OFF)
option(COMPRESSED_IMAGE_BUILD_TESTS OFF)
option(COMPRESSED_IMAGE_BUILD_EXAMPLES OFF)
option(COMPRESSED_IMAGE_BUILD_DOCS OFF)
option(COMPRESSED_IMAGE_BUILD_BENCHMARKS OFF)
option(COMPRESSED_IMAGE_BUILD_PYTHON OFF)

option(_COMPRESSED_IMAGE_SANITIZE_FLAGS "Internal CI flag for enabling sanitizer builds" OFF)


# Add thirdparty libraries
# --------------------------------------------------------------------------

# Add c-blosc2
set(DEACTIVATE_ZLIB ON)
set(BUILD_TESTS OFF)
set(BUILD_FUZZERS OFF)
set(BUILD_BENCHMARKS OFF)
set(BUILD_EXAMPLES OFF)
add_subdirectory (thirdparty/c-blosc2)

# Add target for blosc2 headers
add_library(blosc2_include INTERFACE)
target_include_directories(blosc2_include SYSTEM INTERFACE thirdparty/c-blosc2/include)

# JSON module for parsing/storing metadata
add_subdirectory(thirdparty/json)

# Include the local vcpkg toolchain, if requested, otherwise it is up to the user
# to provide a valid OpenImageIO library (that can be found via find_package)
if (COMPRESSED_IMAGE_USE_VCPKG)
    include("${PROJECT_SOURCE_DIR}/thirdparty/vcpkg/scripts/buildsystems/vcpkg.cmake")
endif()

find_package(OpenImageIO CONFIG QUIET)
if (OpenImageIO_FOUND)
    message(STATUS "Found OpenImageIO")
    set(COMPRESSED_IMAGE_HAVE_OIIO TRUE)
else()
    message(WARNING "OpenImageIO not found, some features will not be available")
    set(COMPRESSED_IMAGE_HAVE_OIIO TRUE)
endif()

# Projects
# --------------------------------------------------------------------------
add_subdirectory(compressed_image)

if (COMPRESSED_IMAGE_BUILD_TESTS)
    add_library(doctest INTERFACE)
    target_include_directories(doctest SYSTEM INTERFACE thirdparty/doctest/doctest)

    add_subdirectory(test)
endif()

if (COMPRESSED_IMAGE_BUILD_EXAMPLES)
    add_subdirectory(examples/read_from_file)
    add_subdirectory(examples/read_with_postprocess)
    add_subdirectory(examples/lazy_channels)
    add_subdirectory(examples/modifying_image)
endif()

if (COMPRESSED_IMAGE_BUILD_BENCHMARKS)
    set(BENCHMARK_ENABLE_INSTALL OFF)
    set(BENCHMARK_INSTALL_DOCS OFF)
    set(BENCHMARK_ENABLE_TESTING OFF)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF)
    add_subdirectory(thirdparty/benchmark)
    add_subdirectory(benchmark)
endif()

if (COMPRESSED_IMAGE_BUILD_DOCS)
    add_subdirectory(docs)
endif()

if (COMPRESSED_IMAGE_BUILD_PYTHON)
    add_subdirectory(thirdparty/pybind11)
    add_subdirectory(thirdparty/pybind11_image_util)
    add_subdirectory(thirdparty/pybind11_json)
    add_subdirectory(python)
endif()
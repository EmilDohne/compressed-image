cmake_minimum_required (VERSION 3.19)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set (CMAKE_CXX_STANDARD 20)
project (CompressedImageBuild)


set(MAIN_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(MAIN_PROJECT ON)
endif()

if (MAIN_PROJECT)
    set(COMPRESSED_IMAGE_BUILD_TESTS ON)
    set(COMPRESSED_IMAGE_BUILD_EXAMPLES ON)
    set(COMPRESSED_IMAGE_BUILD_DOCS ON)
    set(COMPRESSED_IMAGE_BUILD_BENCHMARKS ON)
endif()


option(COMPRESSED_IMAGE_USE_VCPKG "Whether to use the submodule version of vcpkg to resolve the dependencies instead of system libraries." ON)
option(COMPRESSED_IMAGE_BUILD_TESTS OFF)
option(COMPRESSED_IMAGE_BUILD_EXAMPLES OFF)
option(COMPRESSED_IMAGE_BUILD_DOCS OFF)
option(COMPRESSED_IMAGE_BUILD_BENCHMARKS OFF)


# Add thirdparty libraries
# --------------------------------------------------------------------------

# Add c-blosc2
set(DEACTIVATE_ZLIB ON)
set(DEACTIVATE_ZSTD ON)
set(BUILD_TESTS OFF)
set(BUILD_FUZZERS OFF)
set(BUILD_BENCHMARKS OFF)
set(BUILD_EXAMPLES OFF)
add_subdirectory (thirdparty/c-blosc2)

# Add target for blosc2 headers
add_library(blosc2_include INTERFACE)
target_include_directories(blosc2_include SYSTEM INTERFACE thirdparty/c-blosc2/include)

# JSON module for parsing metadata
add_subdirectory(thirdparty/json)

# Add target for json headers
add_library(nlohmann_json_include INTERFACE)
target_include_directories(nlohmann_json_include SYSTEM INTERFACE thirdparty/json/single_include/nlohmann)

# Include the local vcpkg toolchain, if specified
if (COMPRESSED_IMAGE_USE_VCPKG)
    include("${PROJECT_SOURCE_DIR}/thirdparty/vcpkg/scripts/buildsystems/vcpkg.cmake")
endif()

find_package(OpenImageIO CONFIG QUIET)
if (OpenImageIO_FOUND)
    message(STATUS "Found OpenImageIO")
    add_compile_definitions(COMPRESSED_IMAGE_OIIO_AVAILABLE)
else()
    message(WARNING "OpenImageIO not found, some features will not be available")
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
    add_subdirectory(examples/initialize_from_file)
    add_subdirectory(examples/tmp)
endif()

if (COMPRESSED_IMAGE_BUILD_BENCHMARKS)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF)
    add_subdirectory(thirdparty/benchmark)
    add_subdirectory(benchmark)
endif()

if (COMPRESSED_IMAGE_BUILD_DOCS)
    add_subdirectory(docs)
endif()

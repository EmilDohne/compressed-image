cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

set (CMAKE_CXX_STANDARD 20)

project ("CompressedImage")


# Add thirdparty libraries
# --------------------------------------------------------------------------

# Add c-blosc2
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

# Projects
# --------------------------------------------------------------------------
add_subdirectory (CompressedImage)


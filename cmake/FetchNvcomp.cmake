# FetchNvcomp.cmake
# Standalone CMake file to fetch nvcomp headers and provide an INTERFACE target

include(FetchContent)

# Determine platform-specific URL and archive
if(WIN32)
    set(NVCOMP_URL "https://developer.download.nvidia.com/compute/nvcomp/redist/nvcomp/windows-x86_64/nvcomp-windows-x86_64-5.0.0.6_cuda11-archive.zip")
    set(NVCOMP_INCLUDE_DIR "nvcomp-windows-x86_64-5.0.0.6_cuda11-archive/include")
    set(NVCOMP_SHA256 "5C2E1EE55398F47D28806EB7C53ACA33B9E22D6D5B3ACEC86BBC4253C7E6D1D3")
    set(NVCOMP_PLATFORM "Windows")
elseif(UNIX)
    set(NVCOMP_URL "https://developer.download.nvidia.com/compute/nvcomp/redist/nvcomp/linux-x86_64/nvcomp-linux-x86_64-5.0.0.6_cuda11-archive.tar.xz")
    set(NVCOMP_INCLUDE_DIR "nvcomp-linux-x86_64-5.0.0.6_cuda11-archive/include")
    set(NVCOMP_SHA256 "64F5F7CC622F36006C503EE5A3F9D730B5C6CC49E4FAB0FC0507C1272D5EFA7B")
    set(NVCOMP_PLATFORM "Linux")
else()
    message(FATAL_ERROR "Unsupported platform for NVCOMP")
endif()

message(STATUS "Fetching NVCOMP headers for ${NVCOMP_PLATFORM}...")
# Fetch NVCOMP archive
FetchContent_Declare(
    nvcomp_headers
    URL ${NVCOMP_URL}
    URL_HASH SHA256=NVCOMP_SHA256
)

message(STATUS "Downloading and extracting NVCOMP headers (this may take a moment)...")
FetchContent_MakeAvailable(nvcomp_headers)
message(STATUS "NVCOMP headers are now available at ${nvcomp_headers_SOURCE_DIR}/${NVCOMP_INCLUDE_DIR}")


# Create an INTERFACE target for NVCOMP headers
add_library(nvcomp_headers INTERFACE)
target_include_directories(
    nvcomp_headers 
    INTERFACE
    ${nvcomp_headers_SOURCE_DIR}/${NVCOMP_INCLUDE_DIR}
)
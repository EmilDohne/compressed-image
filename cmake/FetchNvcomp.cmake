# FetchNvcomp.cmake
# Fetch NVCOMP headers and static library, provide namespaced targets:
#   compressed::nvcomp_headers
#   compressed::nvcomp (static)

include(FetchContent)

# --- Platform-specific URL, archive, paths ---
if(WIN32)
    set(NVCOMP_URL "https://developer.download.nvidia.com/compute/nvcomp/redist/nvcomp/windows-x86_64/nvcomp-windows-x86_64-5.0.0.6_cuda11-archive.zip")
    set(NVCOMP_LIB_SUBDIR "lib/nvcomp_static.lib")
    set(NVCOMP_SHA256 "5C2E1EE55398F47D28806EB7C53ACA33B9E22D6D5B3ACEC86BBC4253C7E6D1D3")
elseif(UNIX)
    set(NVCOMP_URL "https://developer.download.nvidia.com/compute/nvcomp/redist/nvcomp/linux-x86_64/nvcomp-linux-x86_64-5.0.0.6_cuda11-archive.tar.xz")
    set(NVCOMP_LIB_SUBDIR "lib/libnvcomp_static.a")
    set(NVCOMP_SHA256 "64F5F7CC622F36006C503EE5A3F9D730B5C6CC49E4FAB0FC0507C1272D5EFA7B")
else()
    message(FATAL_ERROR "Unsupported platform for NVCOMP")
endif()

# --- Fetch NVCOMP archive ---
message(STATUS "Fetching nvcomp for ${CMAKE_SYSTEM_NAME}...")
FetchContent_Declare(
    _nvcomp_src
    URL ${NVCOMP_URL}
    URL_HASH SHA256=${NVCOMP_SHA256}
)
FetchContent_MakeAvailable(_nvcomp_src)

# --- Headers interface ---
add_library(compressed_nvcomp_headers INTERFACE)
target_include_directories(compressed_nvcomp_headers INTERFACE
    $<BUILD_INTERFACE:${_nvcomp_src_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
add_library(compressed::nvcomp_headers ALIAS compressed_nvcomp_headers)
message(STATUS "Set nvcomp include dir to ${_nvcomp_src_SOURCE_DIR}/include")

# --- Imported static library ---
add_library(compressed_nvcomp STATIC IMPORTED GLOBAL)
set_target_properties(compressed_nvcomp PROPERTIES
    IMPORTED_LOCATION "${_nvcomp_src_SOURCE_DIR}/${NVCOMP_LIB_SUBDIR}"
    INTERFACE_INCLUDE_DIRECTORIES "${_nvcomp_src_SOURCE_DIR}/include"
)
add_library(compressed::nvcomp ALIAS compressed_nvcomp)

# --- Install headers and library ---
install(DIRECTORY ${_nvcomp_src_SOURCE_DIR}/include/
    DESTINATION include
)
install(FILES
    "${_nvcomp_src_SOURCE_DIR}/${NVCOMP_LIB_SUBDIR}"
    DESTINATION lib
)

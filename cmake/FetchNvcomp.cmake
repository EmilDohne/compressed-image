# FetchNvcomp.cmake
# Fetch NVCOMP headers and dynamic libraries for runtime loading.
# Provides namespaced target:
#   compressed::nvcomp_headers

include(FetchContent)

##############################################################
# Fetch dynamically from NVIDIA Redistributables
##############################################################

if (WIN32)
   set(NVCOMP_URL "https://developer.download.nvidia.com/compute/nvcomp/redist/nvcomp/windows-x86_64/nvcomp-windows-x86_64-5.0.0.6_cuda11-archive.zip")
   set(NVCOMP_SHA256 "5C2E1EE55398F47D28806EB7C53ACA33B9E22D6D5B3ACEC86BBC4253C7E6D1D3")
elseif (UNIX AND NOT APPLE)
   set(NVCOMP_URL "https://developer.download.nvidia.com/compute/nvcomp/redist/nvcomp/linux-x86_64/nvcomp-linux-x86_64-5.0.0.6_cuda11-archive.tar.xz")
   set(NVCOMP_SHA256 "64F5F7CC622F36006C503EE5A3F9D730B5C6CC49E4FAB0FC0507C1272D5EFA7B")
else ()
   message(FATAL_ERROR "Unsupported platform for NVCOMP")
endif ()

FetchContent_Declare(_nvcomp_src
   URL ${NVCOMP_URL}
   URL_HASH SHA256=${NVCOMP_SHA256}
)
FetchContent_MakeAvailable(_nvcomp_src)
set(NVCOMP_ROOT ${_nvcomp_src_SOURCE_DIR})

##############################################################
# Locate Runtime Binaries
##############################################################

if (WIN32)
   file(GLOB FOUND_BINARIES "${NVCOMP_ROOT}/bin/*.dll")
else ()
   file(GLOB FOUND_BINARIES "${NVCOMP_ROOT}/lib/libnvcomp.so*")
endif ()

set(NVCOMP_RUNTIME_BINARIES "${FOUND_BINARIES}" CACHE INTERNAL "nvcomp runtime binaries")

##############################################################
# Set up Compile-Time Header Targets
##############################################################

add_library(compressed_nvcomp_headers INTERFACE)

target_include_directories(compressed_nvcomp_headers INTERFACE
   $<BUILD_INTERFACE:${NVCOMP_ROOT}/include>
   $<INSTALL_INTERFACE:include>
)

add_library(compressed::nvcomp_headers ALIAS compressed_nvcomp_headers)

##############################################################
# Install Rules (For Deployment / Packaging)
##############################################################

install(DIRECTORY ${NVCOMP_ROOT}/include/ DESTINATION include)

if (WIN32)
   install(FILES ${NVCOMP_RUNTIME_BINARIES} DESTINATION bin)
else ()
   install(FILES ${NVCOMP_RUNTIME_BINARIES} DESTINATION lib)
endif ()
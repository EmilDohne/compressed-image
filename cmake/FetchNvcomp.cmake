# FetchNvcomp.cmake
# Fetch NVCOMP headers and static library, provide namespaced targets:
#   compressed::nvcomp_headers
#   compressed::nvcomp (static)

include(FetchContent)

##############################################################
# Check if there is already system targets
##############################################################

find_package(nvcomp CONFIG QUIET)

if (nvcomp_FOUND)
   message(STATUS "Found system nvcomp package")

   add_library(compressed::nvcomp ALIAS nvcomp::nvcomp_static)
   add_library(compressed::nvcomp_headers INTERFACE)
   target_link_libraries(compressed::nvcomp_headers INTERFACE nvcomp::nvcomp_static)

   return()
endif ()


##############################################################
# Fetch dynamically
##############################################################

if (WIN32)
   set(NVCOMP_URL "https://developer.download.nvidia.com/compute/nvcomp/redist/nvcomp/windows-x86_64/nvcomp-windows-x86_64-5.0.0.6_cuda11-archive.zip")
   set(NVCOMP_LIB_SUBDIR "lib/nvcomp_static.lib")
   set(NVCOMP_SHA256 "5C2E1EE55398F47D28806EB7C53ACA33B9E22D6D5B3ACEC86BBC4253C7E6D1D3")
elseif (UNIX)
   set(NVCOMP_URL "https://developer.download.nvidia.com/compute/nvcomp/redist/nvcomp/linux-x86_64/nvcomp-linux-x86_64-5.0.0.6_cuda11-archive.tar.xz")
   set(NVCOMP_LIB_SUBDIR "lib/libnvcomp_static.a")
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
# Set up targets
##############################################################


find_package(CUDAToolkit REQUIRED)

add_library(compressed_nvcomp_headers INTERFACE)

target_include_directories(compressed_nvcomp_headers INTERFACE
   $<BUILD_INTERFACE:${NVCOMP_ROOT}/include>
   $<INSTALL_INTERFACE:include>
)

add_library(compressed::nvcomp_headers ALIAS compressed_nvcomp_headers)
add_library(compressed_nvcomp STATIC IMPORTED GLOBAL)
set_target_properties(compressed_nvcomp PROPERTIES
   IMPORTED_LOCATION "${NVCOMP_ROOT}/${NVCOMP_LIB_SUBDIR}"
   INTERFACE_INCLUDE_DIRECTORIES "${NVCOMP_ROOT}/include"
)
target_compile_definitions(compressed_nvcomp
   INTERFACE
   NVCOMP_STATIC_DEFINE
)

add_library(compressed::nvcomp ALIAS compressed_nvcomp)

target_link_libraries(compressed_nvcomp INTERFACE
   CUDA::cudart
   CUDA::cuda_driver
)

if (MSVC)
   # Propagate iterator debug consistency requirement warning
   target_compile_definitions(compressed_nvcomp INTERFACE
      $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=2>
      $<$<CONFIG:Release>:_ITERATOR_DEBUG_LEVEL=0>
   )

   # Ensure dynamic CRT (nvcomp is built with /MD)
   set_property(TARGET compressed_nvcomp PROPERTY
      MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>"
   )
endif ()

install(DIRECTORY ${NVCOMP_ROOT}/include/
   DESTINATION include
)

install(FILES ${NVCOMP_ROOT}/${NVCOMP_LIB_SUBDIR}
   DESTINATION lib
)
# FetchNvcomp.cmake
# Fetch NVCOMP headers and dynamic libraries for runtime loading.
# Provides namespaced target:
#   compressed::nvcomp_headers

include(FetchContent)

##############################################################
# Fetch dynamically from NVIDIA Redistributables
#
# The CUDA variant (cuda11 / cuda12 / cuda13) is chosen automatically from the
# toolkit resolved in setup_cuda.cmake (COMPRESSED_IMAGE_CUDA_MAJOR), so nvcomp
# always matches the CUDA runtime the rest of the build uses. The nvcomp version
# is tied to the CUDA major because nvcomp 5.2 dropped the cuda11 build:
#   CUDA 11        -> nvcomp 5.0.0.6   (last release with a cuda11 redistributable)
#   CUDA 12 / 13   -> nvcomp 5.2.0.10
##############################################################

if (NOT DEFINED COMPRESSED_IMAGE_CUDA_MAJOR)
   message(FATAL_ERROR "fetch_nvcomp.cmake: include setup_cuda.cmake first (COMPRESSED_IMAGE_CUDA_MAJOR unset).")
endif ()

# Select the pinned version + known-good x86_64 SHA256 hashes for the resolved
# CUDA major. Hashes are the sha256 fields from NVIDIA's nvcomp redist JSON.
if (COMPRESSED_IMAGE_CUDA_MAJOR STREQUAL "11")
   set(_nvcomp_pinned_version "5.0.0.6")
   set(_nvcomp_sha_windows "5c2e1ee55398f47d28806eb7c53aca33b9e22d6d5b3acec86bbc4253c7e6d1d3")
   set(_nvcomp_sha_linux   "64f5f7cc622f36006c503ee5a3f9d730b5c6cc49e4fab0fc0507c1272d5efa7b")
elseif (COMPRESSED_IMAGE_CUDA_MAJOR STREQUAL "12")
   set(_nvcomp_pinned_version "5.2.0.10")
   set(_nvcomp_sha_windows "531670d2fea73d3d499b9ea71c56381ef6a619226510d95f7e8ed588234257e1")
   set(_nvcomp_sha_linux   "3412fb302b3319e64fbbbc5b3cfc17148c872c0b939bf439aee82b328d758d09")
elseif (COMPRESSED_IMAGE_CUDA_MAJOR STREQUAL "13")
   set(_nvcomp_pinned_version "5.2.0.10")
   set(_nvcomp_sha_windows "af0a430a2d94981fc60ea13cce9ebe0cc3d814028fce783ea41b5a90716016d4")
   set(_nvcomp_sha_linux   "2dd6c184c79fa5402c9b63a274e778d4b52e8d736ee927da81f07c1f8bed12ff")
else ()
   message(FATAL_ERROR
      "fetch_nvcomp.cmake: no nvcomp mapping for CUDA major '${COMPRESSED_IMAGE_CUDA_MAJOR}' "
      "(expected 11, 12 or 13).")
endif ()

# Allow overriding the version; 'AUTO' (default) uses the pinned version above.
set(COMPRESSED_IMAGE_NVCOMP_VERSION "AUTO" CACHE STRING
   "nvcomp redistributable version to fetch. 'AUTO' picks a version matching the CUDA major.")

if (COMPRESSED_IMAGE_NVCOMP_VERSION STREQUAL "AUTO")
   set(_nvcomp_version "${_nvcomp_pinned_version}")
else ()
   set(_nvcomp_version "${COMPRESSED_IMAGE_NVCOMP_VERSION}")
endif ()

set(_nvcomp_base "https://developer.download.nvidia.com/compute/nvcomp/redist/nvcomp")

if (WIN32)
   set(_nvcomp_platform "windows-x86_64")
   set(_nvcomp_ext "zip")
   set(NVCOMP_SHA256 "${_nvcomp_sha_windows}")
elseif (UNIX AND NOT APPLE)
   set(_nvcomp_platform "linux-x86_64")
   set(_nvcomp_ext "tar.xz")
   set(NVCOMP_SHA256 "${_nvcomp_sha_linux}")
else ()
   message(FATAL_ERROR "Unsupported platform for NVCOMP")
endif ()

set(NVCOMP_URL
   "${_nvcomp_base}/${_nvcomp_platform}/nvcomp-${_nvcomp_platform}-${_nvcomp_version}_cuda${COMPRESSED_IMAGE_CUDA_MAJOR}-archive.${_nvcomp_ext}")

# The pinned hashes only apply to the pinned version; if the user overrode the
# version we cannot vouch for them, so fetch without an integrity check (and warn).
if (NOT _nvcomp_version STREQUAL _nvcomp_pinned_version)
   message(WARNING
      "COMPRESSED_IMAGE_NVCOMP_VERSION=${_nvcomp_version} differs from the pinned "
      "${_nvcomp_pinned_version} for CUDA ${COMPRESSED_IMAGE_CUDA_MAJOR}; downloading nvcomp "
      "without a SHA256 integrity check.")
   set(NVCOMP_SHA256 "")
endif ()

message(STATUS "compressed-image: fetching nvcomp from ${NVCOMP_URL}")

if (NVCOMP_SHA256)
   FetchContent_Declare(_nvcomp_src URL ${NVCOMP_URL} URL_HASH SHA256=${NVCOMP_SHA256})
else ()
   FetchContent_Declare(_nvcomp_src URL ${NVCOMP_URL})
endif ()
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
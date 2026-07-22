# setup_cuda.cmake
# ------------------------------------------------------------------------------
# Centralised, configurable CUDA toolkit + GPU-architecture handling.
#
# The goal is that the build adapts to whatever the user actually has installed,
# instead of hard-coding a CUDA major version or GPU architecture (which is how
# you end up with device code that runs on the machine it was built on but throws
# cudaErrorIllegalAddress on a newer card).
#
# User-facing cache options:
#   COMPRESSED_IMAGE_CUDA_VERSION        "AUTO" (default) or a CUDA major (11/12/13).
#                                        Selects which prebuilt nvcomp redistributable
#                                        is fetched. AUTO follows the detected toolkit.
#   CMAKE_CUDA_ARCHITECTURES             GPU architectures to compile device code for (the
#                                        standard CMake knob). Defaults to "native" (detect the
#                                        local GPU); accepts e.g. "75;86;120" or "all-major".
#                                        On a machine without a GPU (CI), pass an explicit list.
#
# Provides to the rest of the build:
#   COMPRESSED_IMAGE_CUDA_MAJOR          Resolved CUDA major (used to pick the nvcomp variant).
#   CMAKE_CUDA_ARCHITECTURES             Defaulted so every CUDA target inherits it consistently.

# ##############################################################################
# Locate the toolkit (detect what is actually installed)
# ##############################################################################
find_package(CUDAToolkit REQUIRED)

message(STATUS
   "compressed-image: found CUDA Toolkit ${CUDAToolkit_VERSION} "
   "(nvcc: ${CUDAToolkit_NVCC_EXECUTABLE})")

# ##############################################################################
# Resolve the CUDA major used to select matching prebuilt deps (nvcomp)
# ##############################################################################
set(COMPRESSED_IMAGE_CUDA_VERSION "AUTO" CACHE STRING
   "CUDA major version for prebuilt deps (nvcomp). 'AUTO' follows the detected toolkit.")

if (COMPRESSED_IMAGE_CUDA_VERSION STREQUAL "AUTO")
   set(COMPRESSED_IMAGE_CUDA_MAJOR "${CUDAToolkit_VERSION_MAJOR}")
else ()
   set(COMPRESSED_IMAGE_CUDA_MAJOR "${COMPRESSED_IMAGE_CUDA_VERSION}")
   if (NOT COMPRESSED_IMAGE_CUDA_MAJOR STREQUAL CUDAToolkit_VERSION_MAJOR)
      message(WARNING
         "COMPRESSED_IMAGE_CUDA_VERSION=${COMPRESSED_IMAGE_CUDA_MAJOR} does not match the detected "
         "CUDA Toolkit major (${CUDAToolkit_VERSION_MAJOR}). Prebuilt nvcomp will be fetched for "
         "CUDA ${COMPRESSED_IMAGE_CUDA_MAJOR} - make sure that is intentional.")
   endif ()
endif ()

# nvcomp ships cuda11 / cuda12 / cuda13 redistributables (see fetch_nvcomp.cmake
# for the version mapping).
if (NOT COMPRESSED_IMAGE_CUDA_MAJOR MATCHES "^(11|12|13)$")
   message(FATAL_ERROR
      "compressed-image: unsupported CUDA major '${COMPRESSED_IMAGE_CUDA_MAJOR}'. nvcomp "
      "redistributables are only published for CUDA 11, 12 and 13. If toolkit detection is wrong, "
      "set -DCOMPRESSED_IMAGE_CUDA_VERSION=13 (or 12/11) explicitly.")
endif ()

# ##############################################################################
# GPU architecture selection
# ##############################################################################
# Default to 'native' so device code matches the GPU in the build machine. This is
# what prevents "works on the old card, illegal memory access on the new one".
# Override with the standard CMake knob, e.g. -DCMAKE_CUDA_ARCHITECTURES="75;86;120"
# or "all-major"; on a machine without a GPU (CI) pass an explicit list.

# Self-heal a value corrupted by an earlier bug in this file (a multi-argument
# docstring leaked into the value, giving "native;CACHE;STRING;..."). Without this
# the bad value sticks in the CMake cache across reconfigures.
if (DEFINED CMAKE_CUDA_ARCHITECTURES AND CMAKE_CUDA_ARCHITECTURES MATCHES "CACHE|STRING")
   message(WARNING
      "Resetting malformed CMAKE_CUDA_ARCHITECTURES='${CMAKE_CUDA_ARCHITECTURES}' back to 'native'.")
   unset(CMAKE_CUDA_ARCHITECTURES CACHE)
endif ()

# Respect an explicit user value; otherwise default to native so every CUDA target
# (the filter plugins) inherits one consistent architecture list.
if (NOT DEFINED CMAKE_CUDA_ARCHITECTURES OR CMAKE_CUDA_ARCHITECTURES STREQUAL "")
   set(CMAKE_CUDA_ARCHITECTURES "native")
endif ()

# 'native' requires CMake >= 3.24 - fall back to an explicit, reasonably broad list on
# older CMake so configuration still succeeds.
if (CMAKE_CUDA_ARCHITECTURES STREQUAL "native" AND CMAKE_VERSION VERSION_LESS 3.24)
   if (COMPRESSED_IMAGE_CUDA_MAJOR STREQUAL "11")
      set(CMAKE_CUDA_ARCHITECTURES "70;75;80;86")
   else ()
      # CUDA 12 / 13
      set(CMAKE_CUDA_ARCHITECTURES "75;80;86;89;90")
      # Blackwell (sm_120) exists from CUDA 12.8 onwards.
      if (NOT CUDAToolkit_VERSION VERSION_LESS 12.8)
         list(APPEND CMAKE_CUDA_ARCHITECTURES 120)
      endif ()
   endif ()
   message(WARNING
      "CMAKE_CUDA_ARCHITECTURES=native needs CMake >= 3.24 (have ${CMAKE_VERSION}); "
      "falling back to '${CMAKE_CUDA_ARCHITECTURES}'. Pass -DCMAKE_CUDA_ARCHITECTURES=... to override.")
endif ()

message(STATUS "compressed-image: CUDA architectures = ${CMAKE_CUDA_ARCHITECTURES}")

enable_language(CUDA)

# Bytedelta plugin, if this name is changed it should be changed in the dynamic hooking code as well.
add_library(compressed_bytedelta_plugin SHARED bytedelta_kernels.cu)


target_compile_features(compressed_bytedelta_plugin PRIVATE cxx_std_20)
set_target_properties(compressed_bytedelta_plugin PROPERTIES
   CUDA_STANDARD 20
   CUDA_STANDARD_REQUIRED ON
   POSITION_INDEPENDENT_CODE ON
)

target_include_directories(compressed_bytedelta_plugin PRIVATE
   ${CUDAToolkit_INCLUDE_DIRS}
)

# Set warnings and optimization rules strictly aligning with your compiler selections
if (MSVC)
   target_compile_options(compressed_bytedelta_plugin PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/utf-8,/MP>)
else ()
   target_compile_options(compressed_bytedelta_plugin PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=-fPIC>)
endif ()

if (COMPRESSED_IMAGE_EXTENDED_WARNINGS)
   if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
      target_compile_options(compressed_bytedelta_plugin PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=-Wall,-Wextra>)
   elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
      target_compile_options(compressed_bytedelta_plugin PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/W4>)
   endif ()
endif ()

target_link_libraries(compressed_bytedelta_plugin PRIVATE CUDA::cudart)
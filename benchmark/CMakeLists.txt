file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "*.cpp")

# Enable ctest
enable_testing()
add_executable(compressed_image_benchmark ${MY_SOURCES})

if (MSVC)
   target_compile_options(compressed_image_benchmark PRIVATE /MP /utf-8 /arch:AVX2 /bigobj)
endif ()
target_link_libraries(compressed_image_benchmark PRIVATE compressed_image)
target_link_libraries(compressed_image_benchmark PRIVATE benchmark::benchmark)
add_dependencies(compressed_image_benchmark compressed_bytedelta_plugin compressed_shuffle_plugin compressed_delta_plugin compressed_xordelta_plugin compressed_fmap_plugin)


# Copy the images/ folder to the build dir to run the tests
add_custom_command(TARGET compressed_image_benchmark POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E copy_directory
   ${CMAKE_CURRENT_SOURCE_DIR}/images/ $<TARGET_FILE_DIR:compressed_image_benchmark>/images)
add_custom_command(TARGET compressed_image_benchmark POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E echo
   "Finished copying test files to output directory $<TARGET_FILE_DIR:compressed_image_benchmark>/images")

copy_plugin_to_target(compressed_image_benchmark compressed_bytedelta_plugin)
copy_plugin_to_target(compressed_image_benchmark compressed_shuffle_plugin)
copy_plugin_to_target(compressed_image_benchmark compressed_delta_plugin)
copy_plugin_to_target(compressed_image_benchmark compressed_xordelta_plugin)
copy_plugin_to_target(compressed_image_benchmark compressed_fmap_plugin)

if (NVCOMP_RUNTIME_BINARIES)
   add_custom_command(
      TARGET compressed_image_benchmark POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${NVCOMP_RUNTIME_BINARIES}
      $<TARGET_FILE_DIR:compressed_image_benchmark>
      COMMENT "Syncing nvcomp runtime dependencies next to benchmark executable..."
   )
endif ()
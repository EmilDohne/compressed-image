file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

# Enable ctest
enable_testing()
add_executable(compressed_image_test ${MY_SOURCES} "main.cpp")

if (MSVC)
   target_compile_options(compressed_image_test PRIVATE /MP /utf-8 /bigobj)
endif ()
target_link_libraries(compressed_image_test PRIVATE compressed_image)
target_link_libraries(compressed_image_test PRIVATE doctest)
add_dependencies(compressed_image_test compressed_bytedelta_plugin compressed_shuffle_plugin compressed_delta_plugin compressed_xordelta_plugin)

include(CTest)
add_test(test_compressed_image compressed_image_test)

# Copy the images/ folder to the build dir to run the tests
add_custom_command(TARGET compressed_image_test POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E copy_directory
   ${CMAKE_CURRENT_SOURCE_DIR}/images/ $<TARGET_FILE_DIR:compressed_image_test>/images)
add_custom_command(TARGET compressed_image_test POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E echo
   "Finished copying test files to output directory $<TARGET_FILE_DIR:compressed_image_test>/images")

copy_plugin_to_target(compressed_image_test compressed_bytedelta_plugin)
copy_plugin_to_target(compressed_image_test compressed_shuffle_plugin)
copy_plugin_to_target(compressed_image_test compressed_delta_plugin)
copy_plugin_to_target(compressed_image_test compressed_xordelta_plugin)

if (NVCOMP_RUNTIME_BINARIES)
   add_custom_command(
      TARGET compressed_image_test POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${NVCOMP_RUNTIME_BINARIES}
      $<TARGET_FILE_DIR:compressed_image_test>
      COMMENT "Syncing nvcomp runtime dependencies next to test executable..."
   )
endif ()


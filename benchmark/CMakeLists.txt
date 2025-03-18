file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "*.cpp")

# Enable ctest
enable_testing()
add_executable(compressed_image_benchmark ${MY_SOURCES})

if(MSVC)
    target_compile_options(compressed_image_benchmark PRIVATE /MP /utf-8 /arch:AVX2 /O2)
endif()
target_link_libraries(compressed_image_benchmark PRIVATE compressed_image)
target_link_libraries(compressed_image_benchmark PRIVATE benchmark::benchmark)



# Copy the images/ folder to the build dir to run the tests
add_custom_command(TARGET compressed_image_benchmark POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_CURRENT_SOURCE_DIR}/images/ $<TARGET_FILE_DIR:compressed_image_benchmark>/images)
add_custom_command(TARGET compressed_image_benchmark POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E echo 
                    "Finished copying test files to output directory $<TARGET_FILE_DIR:compressed_image_benchmark>/images")
add_executable(read_with_postprocess main.cpp)
target_link_libraries(read_with_postprocess PRIVATE compressed_image)


# Copy the images/ folder to the build dir to run the tests
add_custom_command(TARGET read_with_postprocess POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_CURRENT_SOURCE_DIR}/images/ $<TARGET_FILE_DIR:read_with_postprocess>/images)
add_custom_command(TARGET read_with_postprocess POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E echo 
                    "Finished copying test files to output directory $<TARGET_FILE_DIR:read_with_postprocess>/images")
add_executable(read_from_file main.cpp)
target_link_libraries(read_from_file PRIVATE compressed_image)


# Copy the images/ folder to the build dir to run the tests
add_custom_command(TARGET read_from_file POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_CURRENT_SOURCE_DIR}/images/ $<TARGET_FILE_DIR:read_from_file>/images)
add_custom_command(TARGET read_from_file POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E echo 
                    "Finished copying test files to output directory $<TARGET_FILE_DIR:read_from_file>/images")
add_executable(initialize_from_file main.cpp)
target_link_libraries(initialize_from_file PRIVATE compressed_image)


# Copy the images/ folder to the build dir to run the tests
add_custom_command(TARGET initialize_from_file POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_CURRENT_SOURCE_DIR}/images/ $<TARGET_FILE_DIR:initialize_from_file>/images)
add_custom_command(TARGET initialize_from_file POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E echo 
                    "Finished copying test files to output directory $<TARGET_FILE_DIR:initialize_from_file>/images")
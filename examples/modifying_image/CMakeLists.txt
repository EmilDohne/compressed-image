add_executable(modifying_image main.cpp)
target_link_libraries(modifying_image PRIVATE compressed_image)


# Copy the images/ folder to the build dir to run the tests
add_custom_command(TARGET modifying_image POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_CURRENT_SOURCE_DIR}/images/ $<TARGET_FILE_DIR:modifying_image>/images)
add_custom_command(TARGET modifying_image POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E echo 
                    "Finished copying test files to output directory $<TARGET_FILE_DIR:modifying_image>/images")
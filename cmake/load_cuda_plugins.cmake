function(copy_plugin_to_target EXECUTABLE_TARGET PLUGIN_TARGET)
   add_custom_command(
      TARGET ${EXECUTABLE_TARGET} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:${PLUGIN_TARGET}>
      $<TARGET_FILE_DIR:${EXECUTABLE_TARGET}>
      COMMENT "Copying ${PLUGIN_TARGET} next to ${EXECUTABLE_TARGET}..."
   )
endfunction()
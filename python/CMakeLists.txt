cmake_minimum_required(VERSION 3.15...3.26)
project(${SKBUILD_PROJECT_NAME} LANGUAGES CXX)

set(PYBIND11_FINDPYTHON ON)
pybind11_add_module(compressed_image_py bindings/main.cpp)
# Ensure we build as compressed_image and not compressed_image_py
set_target_properties(compressed_image_py PROPERTIES OUTPUT_NAME compressed_image)
target_include_directories(compressed_image_py PRIVATE compressed_image bindings)
target_link_libraries(compressed_image_py PUBLIC nlohmann_json pybind11_json py_image_util compressed_image)

install(TARGETS compressed_image_py
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# ------------------------------------------------------------------------------
# Optional: regenerate the .pyi type stubs from the freshly built module so they
# cannot silently drift out of sync with the bindings. This is opt-in (not part
# of ALL) because it needs the module's runtime dependencies (nvcomp, the filter
# plugins and OpenImageIO) importable, plus `pip install pybind11-stubgen`.
#
#   cmake --build <build-dir> --target compressed_image_stubs
#
# It writes the generated stub tree under _stubs/_generated so you can diff it
# against the hand-maintained _stubs/_compressed_image.pyi and copy across what
# changed (the package uses a custom _stubs layout that stubgen does not mirror
# 1:1, so this is a review aid rather than a drop-in overwrite).
# ------------------------------------------------------------------------------
find_program(PYBIND11_STUBGEN_EXECUTABLE pybind11-stubgen)
if (PYBIND11_STUBGEN_EXECUTABLE)
    add_custom_target(compressed_image_stubs
        COMMAND ${CMAKE_COMMAND} -E env
                "PYTHONPATH=$<TARGET_FILE_DIR:compressed_image_py>"
                ${PYBIND11_STUBGEN_EXECUTABLE} compressed_image
                -o ${CMAKE_CURRENT_SOURCE_DIR}/py_module/compressed_image/_stubs/_generated
        DEPENDS compressed_image_py
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Regenerating .pyi stubs via pybind11-stubgen (into _stubs/_generated for review)"
        VERBATIM)
else ()
    message(STATUS "pybind11-stubgen not found; 'compressed_image_stubs' target unavailable "
                   "(pip install pybind11-stubgen to enable stub regeneration).")
endif ()
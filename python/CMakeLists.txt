cmake_minimum_required(VERSION 3.15...3.26)
project(${SKBUILD_PROJECT_NAME} LANGUAGES CXX)

set(PYBIND11_FINDPYTHON ON)
pybind11_add_module(compressed_image_py compressed_image/main.cpp)
# Ensure we build as compressed_image and not compressed_image_py
set_target_properties(compressed_image_py PROPERTIES OUTPUT_NAME compressed_image)
target_include_directories(compressed_image_py PRIVATE compressed_image)
target_link_libraries(compressed_image_py PUBLIC nlohmann_json pybind11_json py_image_util compressed_image)

install(TARGETS compressed_image_py
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
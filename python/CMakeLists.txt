cmake_minimum_required(VERSION 3.15...3.26)
project(${SKBUILD_PROJECT_NAME} LANGUAGES CXX)

set(PYBIND11_FINDPYTHON ON)
pybind11_add_module(compressed_image_py compressed_image/main.cpp)
target_link_libraries(compressed_image_py py_image_util)

install(TARGETS compressed_image_py LIBRARY DESTINATION .)
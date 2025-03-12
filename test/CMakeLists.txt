file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

# Enable ctest
enable_testing()
add_executable(compressed_image_test ${MY_SOURCES})

if(MSVC)
    target_compile_options(compressed_image_test PRIVATE /MP /utf-8)
endif()
target_link_libraries(compressed_image_test INTERFACE compressed_image)
target_link_libraries(compressed_image_test PRIVATE doctest)

include(CTest)
add_test(TestCompressedImage compressed_image_test)
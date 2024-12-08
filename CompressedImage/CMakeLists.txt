# CMakeList.txt : CMake project for CompressedImage, include source and define
# project specific logic here.
#

# Add source to this project's executable.
project(CompressedImage)
file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.cpp")

add_library(CompressedImage STATIC ${MY_SOURCES})

target_include_directories(CompressedImage PUBLIC "include")
target_link_libraries(CompressedImage PUBLIC blosc2_static blosc2_include, nlohmann_json_include)



if (MSVC)
	target_compile_options(CompressedImage PRIVATE /MP /DNOMINMAX)
	target_compile_options(CompressedImage PUBLIC /utf-8)
	# Bump up warning levels and enable the following extra exceptions:
	# w44062 is for if a switch misses some enum members.
	# w44464 is for #include containing .. in the path
	# w45264 is for using std:move when returning a temporary.
	target_compile_options(CompressedImage PRIVATE /W4 /WX /w44062 /w44464 /w45264)
endif()

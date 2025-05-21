project(CompressedImage)

add_library(compressed_image INTERFACE)
target_include_directories(compressed_image INTERFACE "include")
target_link_libraries(compressed_image INTERFACE  
	blosc2_static 
	blosc2_include 
	nlohmann_json_include 
	OpenImageIO::OpenImageIO
)

if (MSVC)
	target_compile_options(compressed_image INTERFACE /utf-8 /MP /DNOMINMAX)
endif()


if (COMPRESSED_IMAGE_HAVE_OIIO)
	target_compile_definitions(compressed_image INTERFACE COMPRESSED_IMAGE_OIIO_AVAILABLE)
endif()


# Crank up warning levels on both MSVC, Clang and GCC
if (COMPRESSED_IMAGE_EXTENDED_WARNINGS)
	if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
	target_compile_options(
		compressed_image 
		INTERFACE 
		-Wall 
		-Werror 
		-Wextra
	)
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	target_compile_options(
		compressed_image 
		INTERFACE 
		/W4 
		/WX 
		/w44062 
		/w44464 
		/w45264
	)
	endif()
endif()

# Enable sanitizers unless on macOS (not supported) or Windows (github runners run out of memory). 
# These are for our CI runs only and should be ignored by users.
if (_COMPRESSED_IMAGE_SANITIZE_FLAGS AND NOT APPLE)
	if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
		target_compile_options(compressed_image INTERFACE -fsanitize=address,leak,undefined)
		target_link_options(compressed_image INTERFACE -fsanitize=address,leak,undefined)
	endif()
endif()